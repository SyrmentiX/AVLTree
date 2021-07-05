#pragma once

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

	enum class status {
		DELETED = 0,
		ACTIVE = 1,
		END = 2,
		BEGIN = 3
	};

	static std::atomic<int> calls = 0;

	template <typename T>
	class list_node {
	private:
		template <typename G>
			friend class ListIterator;

		template <typename G>
			friend class List;

		std::atomic<status> node_status;
		std::atomic<std::size_t> ref_count = 0;
		std::shared_mutex mutex;
		T value;
		list_node *left, *right, *purge_left;
		
		list_node(status state) : node_status(state), value(){
			left = nullptr;
			right = nullptr;
			purge_left = nullptr;
		}

		list_node(T value, status state) : list_node(state){
			this->value = value;
		}

		list_node(T value) : value(value), node_status(status::ACTIVE) {
			left = nullptr;
			right = nullptr;
			purge_left = nullptr;
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
			std::unique_lock<std::shared_mutex> lock(mutex);
			decrease_ref();
			if (ref_count == 0 && node_status == status::DELETED) {
				lock.unlock();
				this->remove();
			}
		}

		void remove() {
			std::vector<list_node<T>*> stack;
			stack.push_back(left);
			stack.push_back(right);
			
			delete this;
			++calls; // debug info

			while (!stack.empty()) {
				auto* unit = stack.back();
				stack.pop_back();
				if (unit) {
					std::unique_lock<std::shared_mutex> lock(unit->mutex);
					unit->decrease_ref();
					if (!unit->is_ref() && unit->node_status == status::DELETED) {
						stack.push_back(unit->left);
						stack.push_back(unit->right);
						delete unit;

						++calls; // debug info
					}
				}
			}
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
		}
		
		~ListIterator() {
			value->release();
		}

		ListIterator& operator=(const ListIterator& other) {
			std::unique_lock<std::shared_mutex> lock(value->mutex);
			std::unique_lock<std::shared_mutex> lock_other;
			if (value != other.value) {
				lock_other = std::unique_lock<std::shared_mutex>(other.value->mutex);
			}
			auto* prev_value = value;
			value = other.value;
			value->increase_ref();

			lock_other.unlock();
			prev_value->release();

			return *this;
		}

		ListIterator& operator=(ListIterator&& other) {

			std::unique_lock<std::shared_mutex> lock(value->mutex);
			std::unique_lock<std::shared_mutex> lock_other;
			if (value != other.value) {
				lock_other = std::unique_lock<std::shared_mutex>(other.value->mutex);
			}
			auto* prev_value = value;
			value = other.value;
			value->increase_ref();
			
			lock_other.unlock();
			prev_value->release();

			return *this;
		}

		value_type get() {
			std::shared_lock<std::shared_mutex> lock(this->value->mutex);
			return (this->value) ? this->value->value : value_type();
		}

		list_node<value_type>* get_pointer() {
			return value;
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

		bool operator==(const ListIterator& other) {
			return other.value == this->value;
		}

		bool operator!=(const ListIterator& other) {
			return other.value != this->value;
		}

	private:
		list_node<value_type>* value = nullptr;

		void inner_plus() {
			if (value != nullptr && value->node_status != status::END) {
				list_node<value_type>* prev_value = nullptr;
				{
					std::shared_lock<std::shared_mutex> lock_cur(value->mutex);
					auto* right = value->right;

					prev_value = value;
					value = value->right;
					value->increase_ref();
				}
				prev_value->release();
			}
		}

		ListIterator(list_node<value_type>* value) noexcept {
			this->value = value;
			this->value->increase_ref();
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

			List(std::initializer_list<list_type> list) : List() {
				push_back(list);
			}

			List() {
				last = new value_type(status::END);
				root = new value_type(status::BEGIN);
				last->left = root;
				root->right = last;
			}

			~List() {
				value_type* current = root;
				std::vector<value_type*> stack;
				while (current != last) {
					stack.push_back(current);
					current = current->right;
				}
				while (!stack.empty()) {
					delete stack.back();
					stack.back() = nullptr;
					stack.pop_back();
				}

				delete current;
				current = nullptr;
			}

			bool empty() {
				return list_size == 0;
			}

			size_type size() {
				return list_size;
			}

			iterator begin() {
				std::unique_lock<std::shared_mutex> lock_root(root->mutex);
				std::unique_lock<std::shared_mutex> lock(root->right->mutex);
				return iterator(root->right);
			}

			iterator end() {
				std::unique_lock<std::shared_mutex> lock = std::unique_lock<std::shared_mutex>(last->mutex);
				return iterator(last);
			}

			void push_front(list_type value) {
				push_front_inner(value);
			}

			void push_back(list_type value) {
				push_back_inner(value);
			}

			void insert(iterator& it, list_type value) {
				insert_inner(it, value);
			}

			iterator find(list_type value) {
				return find_inner(value);
			}

			void erase(iterator &it) {
				erase_inner(it);
			}

			void pop_back() {
				if (!empty()) {
					std::unique_lock<std::shared_mutex> lock = std::unique_lock<std::shared_mutex>(last->mutex);
					auto it = iterator(last->left);
					lock.unlock();
					erase_inner(it);
				}
			}

		private:
			value_type* root = nullptr;
			value_type* last = nullptr;
			value_type* purgatory = nullptr;
			std::atomic<size_type> list_size = 0;

			void push_front_inner(list_type value) {
				std::unique_lock<std::shared_mutex> lock_root_write(root->mutex);
				auto* rightR = root->right;
				std::unique_lock<std::shared_mutex> lock_right(rightR->mutex);

				value_type* new_node = new value_type(value);
				new_node->left = root;
				new_node->right = rightR;

				rightR->left = new_node;
				root->right = new_node;
				++list_size;
			}

			void push_back_inner(list_type value) {
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
							value_type* new_node = new value_type(value);
							new_node->left = left;
							new_node->right = last;

							left->right = new_node;
							last->left = new_node;

							++list_size;

							retry = false;
						} 
						
					}
					
					left->release();
				}
			}

			void insert_inner(iterator& it, list_type value) {
				value_type* left = it.value;
				if (left->node_status == status::END) {
					push_back_inner(value);
				} else if (left->node_status == status::BEGIN) {
					push_front_inner(value);
				} else {
					std::unique_lock<std::shared_mutex> lock_left(left->mutex);
					if (left->node_status == status::DELETED)
						return;

					value_type* right = left->right;
					std::unique_lock<std::shared_mutex> lock_right(right->mutex);

					value_type* new_node = new value_type(value);
					new_node->left = left;
					new_node->right = right;

					left->right = new_node;
					last->left = new_node;

					++list_size;
				}
			}

			iterator find_inner(list_type value) {
				value_type* current = root;
				std::shared_lock<std::shared_mutex> guard_cur(current->mutex);
				current = current->right;
				while (current != last && current->value != value) {
					guard_cur = std::shared_lock<std::shared_mutex>(current->mutex);
					current = current->right;
				}
				return iterator(current);
			}

			void erase_inner(iterator& it) {
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

							left->right = right;
							right->left = left;

							node->node_status = status::DELETED;
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
	};
}