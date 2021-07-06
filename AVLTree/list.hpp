#pragma once

#include <iostream>
#include <future>
#include <thread>
#include <ctime>
#include <condition_variable>
#include <functional>
#include <memory>
#include <new>
#include <utility>
#include <type_traits>
#include <numeric>
#include <vector>
#include <atomic>
#include <shared_mutex>

namespace fefu {

	template <typename T> class List;


	enum class status {
		DELETED = 0,
		ACTIVE = 1,
		END = 2,
		BEGIN = 3
	};

	template <typename T>
	class list_node {
	private:
		template <typename G>
		friend class ListIterator;

		template <typename G>
		friend class List;

		template <typename G>
		friend class Purgatory;

		std::atomic<status> node_status;
		T value;
		List<T>* list = nullptr;
		list_node* left, * right;
		std::atomic<std::size_t> ref_count = 0;
		std::atomic<int> purged = 0;
		std::shared_mutex mutex;

		list_node(status state, List<T>* list) : node_status(state), value(), list(list) {
			left = nullptr;
			right = nullptr;
		}

		list_node(status state, T value, List<T>* list) : list_node(state) {
			this->value = value;
			this->list = list;
		}

		list_node(T value, List<T>* list) : node_status(status::ACTIVE), value(value), list(list) {
			left = nullptr;
			right = nullptr;
		}

		bool is_ref() {
			return ref_count != 0;
		}

		void increase_ref() {
			++ref_count;
		}

		void decrease_ref() {
			if (ref_count != 0) {
				--ref_count;
			}
		}

		void release() {
			std::shared_lock<std::shared_mutex> lock(list->purge_mutex);
			auto prev_ref = --ref_count;
			if (prev_ref == 0) {
				list->purgatory->push_to_purge(this);
			}
		}
	};

	template <typename T>
	class purgatory_node {
	private:
		template<typename G>
		friend class Purgatory;

		purgatory_node(list_node<T>* node) : node(node), next(nullptr) {}

		list_node<T>* node;
		purgatory_node<T>* next;
	};

	template <typename T>
	class Purgatory {
	private:
		List<T>* list;
		std::atomic<purgatory_node<T>*> head = nullptr;
		std::thread purgeThread;
		std::atomic<bool> purg_cleared = false;

		template<typename G>
		friend class List;

		template<typename G>
		friend class list_node;

		Purgatory(List<T>* list) : list(list) {
			purgeThread = std::thread(&Purgatory::clear_purgatory, this);
		}

		~Purgatory() {
			purg_cleared = true;
			purgeThread.join();
		}

		void push_to_purge(list_node<T>* node) {
			purgatory_node<T>* pnode = new purgatory_node<T>(node);

			do {
				pnode->next = head.load();
			} while (!head.compare_exchange_strong(pnode->next, pnode));
		}

		void remove_node(purgatory_node<T>* prev, purgatory_node<T>* node) {
			prev->next = node->next;
			delete node;
		}

		void release_node(purgatory_node<T>* node) {
			list_node<T>* left = node->node->left;
			list_node<T>* right = node->node->right;

			if (left) {
				left->release();
			}
			if (right) {
				right->release();
			}

			delete node->node;
			delete node;
		}

		void clear_purgatory() {
			do {
				std::unique_lock<std::shared_mutex> lock(list->purge_mutex);
				purgatory_node<T>* head1 = this->head;
				lock.unlock();

				if (head1) {
					purgatory_node<T>* prev_node = head1;
					for (purgatory_node<T>* next_node = head1; next_node;) {
						purgatory_node<T>* current_node = next_node;
						next_node = next_node->next;

						if (current_node->node->is_ref() || current_node->node->purged == 1) {
							remove_node(prev_node, current_node);
						}
						else {
							++current_node->node->purged;
							prev_node = current_node;
						}
					}

					lock.lock();
					purgatory_node<T>* head2 = this->head;

					if (head1 == head2) {
						head = nullptr;
					}

					lock.unlock();

					prev_node = head2;
					for (purgatory_node<T>* next_node = head2; next_node != head1;) {
						purgatory_node<T>* current_node = next_node;
						next_node = next_node->next;

						if (current_node->node->purged == 1) {
							remove_node(prev_node, current_node);
						}
						else {
							prev_node = current_node;
						}
					}

					prev_node->next = nullptr;

					for (purgatory_node<T>* next_node = head1; next_node;) {
						purgatory_node<T>* current_node = next_node;
						next_node = next_node->next;
						release_node(current_node);
					}
				} 
				if (!purg_cleared) {
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
			} while (!purg_cleared || head.load());
		}
	};

	template <typename T>
	class ListIterator {
	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = T;
		using difference_type = std::ptrdiff_t;
		using reference = value_type&;
		using pointer = value_type*;

		template <typename G>
		friend class List;

		ListIterator(const ListIterator& other) noexcept {
			std::unique_lock<std::shared_mutex> lock(other.value->mutex);
			value = other.value;
			value->increase_ref();
			list = other.list;
		}

		~ListIterator() {
			value->release();
		}

		reference operator*() const {
			std::shared_lock<std::shared_mutex> lock(value->mutex);
			return value->value;
		}

		ListIterator& operator=(const ListIterator& other) {
			std::unique_lock<std::shared_mutex> lock(value->mutex);

			if (value == other.value) {
				return *this;
			}

			std::unique_lock<std::shared_mutex> lock_other(other.value->mutex);

			auto* prev_value = value;
			value = other.value;
			value->increase_ref();
			list = other.list;

			lock_other.unlock();
			lock.unlock();

			prev_value->release();
			return *this;
		}

		ListIterator& operator=(ListIterator&& other) {

			std::unique_lock<std::shared_mutex> lock(value->mutex);

			if (value == other.value) {
				return *this;
			}

			std::unique_lock<std::shared_mutex> lock_other(other.value->mutex);
			auto* prev_value = value;
			value = other.value;
			value->increase_ref();
			list = other.list;

			lock_other.unlock();
			lock.unlock();

			prev_value->release();

			return *this;
		}

		value_type get() {
			std::shared_lock<std::shared_mutex> lock(this->value->mutex);
			return (this->value) ? this->value->value : value_type();
		}

		void set(value_type new_value) {
			std::unique_lock<std::shared_mutex> lock(this->value->mutex);
			this->value->value = new_value;
		}

		ListIterator& operator++() {
			inner_plus();
			return *this;
		}

		ListIterator operator++(int) {
			ListIterator temp = *this;
			inner_plus();
			return temp;
		}

		ListIterator& operator--() {
			inner_minus();
			return *this;
		}

		ListIterator operator--(int) {
			ListIterator temp = *this;
			inner_minus();
			return temp;
		}

		bool operator==(const ListIterator& other) {
			return other.value == this->value;
		}

		bool operator!=(const ListIterator& other) {
			return other.value != this->value;
		}

	private:
		list_node<value_type>* value = nullptr;
		List<T>* list;

		void inner_plus() {
			if (value && value->node_status != status::END) {
				list_node<value_type>* prev_value = nullptr;
				{
					std::shared_lock<std::shared_mutex> lock_cur(list->purge_mutex);

					prev_value = value;
					value = value->right;
					value->increase_ref();
				}
				prev_value->release();
			}
		}

		void inner_minus() {
			if (value && value->node_status != status::BEGIN) {
				list_node<value_type>* prev_value = nullptr;
				{
					std::shared_lock<std::shared_mutex> lock_cur(list->purge_mutex);

					prev_value = value;
					value = value->left;
					value->increase_ref();
				}
				prev_value->release();
			}
		}

		ListIterator(list_node<value_type>* value, List<T>* list) noexcept {
			this->value = value;
			this->value->increase_ref();
			this->list = list;
		}
	};

	template <typename T>
	class List {
	public:
		using size_type = std::size_t;
		using list_type = T;
		using value_type = list_node<list_type>;
		using reference = list_type&;
		using const_reference = const list_type&;
		using iterator = ListIterator<list_type>;

		template <typename G>
		friend class list_node;

		template <typename G>
		friend class Purgatory;

		template <typename G>
		friend class ListIterator;

		List(std::initializer_list<list_type> list) : List() {
			for (auto it : list)
				push_back(it);
		}

		List() {
			last = new value_type(status::END, this);
			root = new value_type(status::BEGIN, this);
			purgatory = new Purgatory<T>(this);

			last->increase_ref();
			root->increase_ref();

			last->left = root;
			root->right = last;
		}

		~List() {
			delete purgatory;

			value_type* current = root;
			while (current != last) {
				value_type* prev = current;
				current = current->right;
				delete prev;
			}
			delete current;
		}

		bool empty() {
			return list_size == 0;
		}

		size_type size() {
			return list_size;
		}

		iterator begin() {
			std::shared_lock<std::shared_mutex> lock_root(root->mutex);
			return iterator(root->right, this);
		}

		iterator end() {
			std::shared_lock<std::shared_mutex> lock(last->mutex);
			return iterator(last, this);
		}

		void push_front(list_type value) {
			std::unique_lock<std::shared_mutex> lock_root_write(root->mutex);
			auto* rightR = root->right;
			std::unique_lock<std::shared_mutex> lock_right(rightR->mutex);

			value_type* new_node = new value_type(value, this);
			new_node->left = root;
			new_node->right = rightR;
			new_node->increase_ref();
			new_node->increase_ref();


			rightR->left = new_node;
			root->right = new_node;
			++list_size;
		}

		void push_back(list_type value) {
			value_type* left = nullptr;
			for (bool retry = true; retry;) {
				{
					std::unique_lock<std::shared_mutex> lock(last->mutex);
					left = last->left;
					left->increase_ref();
				}

				{
					std::unique_lock<std::shared_mutex> lock_left(left->mutex);
					std::unique_lock<std::shared_mutex> lock(last->mutex);

					if (left->right == last && last->left == left) {
						value_type* new_node = new value_type(value, this);
						new_node->left = left;
						new_node->right = last;
						new_node->increase_ref();
						new_node->increase_ref();


						left->right = new_node;
						last->left = new_node;

						++list_size;

						retry = false;
					}

				}

				left->release();
			}
		}

		void insert(iterator& it, list_type value) {
			value_type* left = it.value;
			if (left->node_status == status::END) {
				push_back(value);
			}
			else if (left->node_status == status::BEGIN) {
				push_front(value);
			}
			else {
				std::unique_lock<std::shared_mutex> lock_left(left->mutex);
				if (left->node_status == status::DELETED)
					return;

				value_type* right = left->right;
				std::unique_lock<std::shared_mutex> lock_right(right->mutex);

				value_type* new_node = new value_type(value, this);
				new_node->increase_ref();
				new_node->increase_ref();
				new_node->left = left;
				new_node->right = right;

				left->right = new_node;
				right->left = new_node;

				++list_size;
			}
		}

		iterator find(list_type value) {
			value_type* current = root;
			std::shared_lock<std::shared_mutex> lock(current->mutex);
			current = current->right;
			while (current != last && current->value != value) {
				lock = std::shared_lock<std::shared_mutex>(current->mutex);
				current = current->right;
			}
			return iterator(current, this);
		}

		void erase(iterator it) {
			value_type* node = it.value;

			if (empty() || node->node_status != status::ACTIVE)
				return;

			value_type* left = nullptr;
			value_type* right = nullptr;

			for (bool retry = true; retry;) {
				{
					std::shared_lock<std::shared_mutex> lock(node->mutex);
					if (node->node_status == status::DELETED)
						return;

					left = node->left;
					left->increase_ref();

					right = node->right;
					right->increase_ref();
				}

				{
					std::unique_lock<std::shared_mutex> lock_left(left->mutex);
					std::shared_lock<std::shared_mutex> lock(node->mutex);
					std::unique_lock<std::shared_mutex> lock_right(right->mutex);

					if (left->right == node && right->left == node) {
						node->node_status = status::DELETED;
						node->decrease_ref();
						node->decrease_ref();

						left->right = right;
						right->left = left;

						left->increase_ref();
						right->increase_ref();

						--list_size;
						retry = false;
					}
				}

				left->release();
				right->release();
			}
		}

		void pop_back() {
			if (!empty()) {
				std::unique_lock<std::shared_mutex> lock(last->mutex);
				auto it = iterator(last->left, this);
				lock.unlock();
				erase(it);
			}
		}

	private:
		value_type* root = nullptr;
		value_type* last = nullptr;
		Purgatory<T>* purgatory;
		std::atomic<size_type> list_size = 0;
		std::shared_mutex purge_mutex;
	};
}