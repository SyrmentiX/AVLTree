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

	enum class status {
		DELETED = 0,
		ACTIVE = 1,
		END = 2
	};

	template <typename T, typename K>
	class node {
	public:
		status node_status;
		std::size_t ref_count = 0;
		int height = 0;
		T value;
		K key;
		node *left, *right, *parent;

		node(status state) : node_status(state), value(), key() {
			left = nullptr;
			right = nullptr;
			parent = nullptr;
		}

		node(T value, K key, status state) : node(state), value(value), key(key) {}

		node(T value, K key): value(value), key(key), node_status(status::ACTIVE){
			left = nullptr;
			right = nullptr;
			parent = nullptr;
		}

		node(T value, K key, node* parent) : node(value, key) {
			this->parent = parent;
		}
		
		bool is_ref() {
			return ref_count != 0;
		}

		void increase_ref() {
			if (this) {
				++ref_count;
			}
		}

		void decrease_ref() {
			if (this && ref_count != 0) {
				--ref_count;
			}
		}

		void remove() {
			if (this) {
				std::vector<node*> stack;
				stack.push_back(this);
				while (!stack.empty()) {
					node* unit = stack[stack.size() - 1];
					stack.pop_back();
					if (unit && unit->node_status == status::DELETED && !unit->is_ref()) {
						unit->left->decrease_ref();
						unit->right->decrease_ref();
						unit->parent->decrease_ref();
						stack.push_back(unit->left);
						stack.push_back(unit->right);
						stack.push_back(unit->parent);
						delete unit;
					}
				}
			}
		}
	};

	template <typename T, typename K>
	class AVLIterator {
	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = T;
		using key_type = K;
		using difference_type = std::ptrdiff_t;
		using reference = value_type&;
		using pointer = value_type*;

		template <typename T, typename K,
			typename Compare = std::less<T>>
		friend class AVLTree;

		AVLIterator() noexcept {}

		AVLIterator(const AVLIterator& other, std::shared_mutex* mutex) noexcept {
			value = other.value;
			value->increase_ref();
			this->mutex = mutex;
		}

		~AVLIterator() {
			if (value) {
				value->decrease_ref();
				if (!value->is_ref()) {
					value->remove();
				}
			}
		}

		AVLIterator& operator=(AVLIterator& other) {
			*this = AVLIterator(other);
			return *this;
		}

		AVLIterator& operator=(AVLIterator&& other) {
			value->decrease_ref();
			value->remove();
			value = other.value;
			value->increase_ref();
			return *this;
		}

		reference operator*() const {
			return this->value->value;
		}

		pointer operator->() const {
			return &this->value->value;
		}

		AVLIterator &operator++() {
			if (value->node_status != status::END) {
				node<value_type, key_type>* prev_value = value;
				value->decrease_ref();
				if (value->right) {
					value = value->right;
					while (value->left && prev_value->key < value->left->key) {
						value = value->left;
					}
				} else if (value->parent) {
					if (value->parent->left == value) {
						value = value->parent;
					} else if (value->parent->right == value) {
						while (value->parent != nullptr && value->parent->right == value) {
							value = value->parent;
						}
						value = value->parent;
					} else {
						value = value->parent;
					}
				}
				value->increase_ref();

				if (!prev_value->is_ref() && prev_value->node_status == status::DELETED) {
					prev_value->remove();
				}
			}
			return *this;
		}

		// postfix ++
		AVLIterator operator++(int) {
			AVLIterator temp = *this;
			++*this;
			return temp;
		}

		bool operator==(const AVLIterator& other) {
			return other.value == this->value;
		}

		bool operator!=(const AVLIterator& other) {
			return other.value != this->value;
		}

	private:
		node<value_type, key_type>* value = nullptr;
		std::shared_mutex* mutex;

		AVLIterator(node<value_type, key_type>* value) noexcept : value(value) {
			value->increase_ref();
		}
	};

	template <typename T, typename K,
		typename Compare = std::less<T>>
	class AVLTree {
	public:
		using size_type = std::size_t;
		using height_type = std::size_t;
		using bf_type = int;
		using value_compare = Compare;
		using map_type = T;
		using key_type = K;
		using value_type = node<map_type, key_type>;
		using reference = map_type&;
		using const_reference = const map_type&;
		using iterator = AVLIterator<map_type, key_type>;

		AVLTree(std::initializer_list<std::pair<map_type, key_type>> list) : AVLTree() {
			insert(list);
		}

		AVLTree() {
			root = new value_type(status::END);
		}

		~AVLTree() {
			iterator current = this->begin();
			iterator end = this->end();
			std::vector<value_type*> stack;
			while (current != end) {
				stack.push_back(current.value);
				++current;
			}
			while (!stack.empty()) {
				delete stack[stack.size() - 1];
				stack[stack.size() - 1] = nullptr;
				stack.pop_back();	
			}
			delete current.value;
			current.value = nullptr;
		}

		bool empty() {
			std::shared_lock<std::shared_mutex> lock(mutex);
			return set_size == 0;
		}

		size_type size() {
			std::shared_lock<std::shared_mutex> lock(mutex);
			return set_size;
		}

		iterator begin() {
			std::shared_lock<std::shared_mutex> lock(mutex);
			value_type* current = root;
			while (current->left) {
				current = current->left;
			}
			return iterator(current);
		}

		iterator end() {
			std::shared_lock<std::shared_mutex> lock(mutex);
			value_type* current = root;
			while (current->right) {
				current = current->right;
			}
			return iterator(current);
		}

		void insert(map_type value, key_type key) {
			std::unique_lock<std::shared_mutex> lock(mutex);
			value_type *parent_node = find_node(key);
			if (parent_node->key != key || parent_node->node_status == status::END) {
				value_type* new_node = new value_type(value, key, parent_node);
				if (_compare(new_node->key, parent_node->key)) {
					parent_node->left = new_node;
				} else {
					parent_node->right = new_node;
				}
				if (parent_node->node_status == status::END && parent_node->right) {
					swap(parent_node, new_node);
					new_node = parent_node;
				}
				++set_size;
				balance_insert(new_node);
			}
		}

		void insert(std::pair<map_type, key_type> pair) {
			insert(pair.first, pair.second);
		}

		void insert(std::initializer_list<std::pair<map_type, key_type>> list) {
			for (auto& it : list) {
				insert(it);
			}
		}

		iterator find(key_type key) {
			std::shared_lock<std::shared_mutex> lock(mutex);
			auto it = iterator(find_node(key));
			if (it.value->key != key) {
				return end();
			}
			return it;
		}

		void erase(key_type key) {
			if (!empty()) {
				std::unique_lock<std::shared_mutex> lock(mutex);
				value_type* unit = find_node(key);
				if (unit->key == key && unit->node_status == status::ACTIVE) {
					--set_size;
					value_type* lower_unit = unit;
					
					if (unit->left) {
						lower_unit = get_lower_right_child(unit->left);
					} else if (unit->right) {
						lower_unit = get_lower_left_child(unit->right);
					}

					root = (unit == root) ? lower_unit : root;

					if (lower_unit->parent && lower_unit->parent != unit) {
						if (lower_unit->left) {
							change_parent_child(lower_unit, lower_unit->left, lower_unit->parent);
							lower_unit->left->parent = lower_unit->parent;
						}
						else if (lower_unit->right) {
							change_parent_child(lower_unit, lower_unit->right, lower_unit->parent);
							lower_unit->right->parent = lower_unit->parent;
						}
						else {
							change_parent_child(lower_unit, nullptr, lower_unit->parent);
						}
					}

					auto balanced_unit = (lower_unit->parent == unit) ? lower_unit : lower_unit->parent;
					replace_node(lower_unit, unit);

					while (balanced_unit) {
						balance_delete(balanced_unit);
						balanced_unit = balanced_unit->parent;
					}

					delete_node(unit);
				}
			}
		}

	private:
		value_type *root = nullptr;
		size_type set_size = 0;
		value_compare _compare;
		std::shared_mutex mutex;

		void delete_node(value_type *unit) {
			unit->node_status = status::DELETED;
			if (!unit->is_ref()) {
				delete unit;
				unit = nullptr;
			} else {
				unit->parent->increase_ref();
				unit->left->increase_ref();
				unit->right->increase_ref();
			}
		}

		void change_parent_child(value_type *old_child, value_type *new_child, value_type *parent) {
			if (parent && parent != new_child) {
				if (parent->left == old_child) {
					parent->left = new_child;
				}
				else {
					parent->right = new_child;		
				}
				parent->height = get_height(parent);
			}
		}

		void swap_child(value_type *lhs, value_type *rhs) {
			if (lhs->left && lhs->left != rhs) {
				lhs->left->parent = rhs;
			}
			if (lhs->right && lhs->right != rhs) {
				lhs->right->parent = rhs;
			}
			if (rhs->left && rhs->left != lhs) {
				rhs->left->parent = lhs;
			}
			if (rhs->right && rhs->right != lhs) {
				rhs->right->parent = lhs;
			}

			auto *temp = rhs->left;
			rhs->left = (lhs->left == rhs) ? lhs : lhs->left;
			lhs->left = (temp == lhs) ? rhs : temp;

			temp = rhs->right;
			rhs->right = (lhs->right == rhs) ? lhs : lhs->right;
			lhs->right = (temp == lhs) ? rhs : temp;
		}

		void swap_parent(value_type *lhs, value_type *rhs) {
			auto *temp = rhs->parent;
			rhs->parent = (lhs->parent == rhs) ? lhs : lhs->parent;
			lhs->parent = (temp == lhs) ? rhs : temp;
		}

		void swap_height(value_type* lhs, value_type* rhs) {
			auto height = lhs->height;
			lhs->height = rhs->height;
			rhs->height = height;
		}

		void swap(value_type *lhs, value_type *rhs) {
			if (lhs != rhs) {
				change_parent_child(lhs, rhs, lhs->parent);
				change_parent_child(rhs, lhs, rhs->parent);
				swap_child(lhs, rhs);
				swap_parent(lhs, rhs);
				swap_height(lhs, rhs);

				if (!rhs->parent) {
					root = rhs;
				}
				else if (!lhs->parent) {
					root = lhs;
				}
			}
		}

		void replace_node(value_type* lhs, value_type* rhs) {
			if (lhs != rhs) {
				if (rhs->left && rhs->left != lhs) {
					rhs->left->parent = lhs;
				}
				if (rhs->right && rhs->right != lhs) {
					rhs->right->parent = lhs;
				}

				if (rhs->left == lhs) {
					lhs->left = (lhs->left) ? lhs->left : lhs->right;
				}
				else {
					lhs->left = rhs->left;
				}

				if (rhs->right == lhs) {
					lhs->right = (lhs->left) ? lhs->left : lhs->right;
				}
				else {
					lhs->right = rhs->right;
				}
				lhs->height = get_height(lhs);

				change_parent_child(rhs, lhs, rhs->parent);
				lhs->parent = rhs->parent;
			}
		}

		value_type *get_lower_right_child(value_type *unit) {
			while (unit->right)
				unit = unit->right;
			return unit;
		}

		value_type *get_lower_left_child(value_type *unit) {
			while (unit->left)
				unit = unit->left;
			return unit;
		}

		height_type get_height(value_type *unit) {
			if (unit->left && unit->right) {
				if (unit->left->height < unit->right->height) {
					return unit->right->height + 1;
				} else {
					return  unit->left->height + 1;
				}
			} else if (unit->left && unit->right == nullptr) {
				return unit->left->height + 1;
			} else if (unit->left == nullptr && unit->right) {
				return unit->right->height + 1;
			}
			return 0;
		}

		bf_type get_bf(value_type* unit) {
			if (unit == nullptr) {
				return 0;
			}
			if (unit->left && unit->right) {
				return unit->left->height - unit->right->height;
			} else if (unit->left && unit->right == nullptr) {
				return unit->height;
			} else if (unit->left == nullptr && unit->right) {
				return -unit->height;
			}
			return 0;
		}

		void left_leftRotate(value_type *unit) {
			value_type *child = unit->left;

			if (unit->parent != nullptr) {
				change_parent_child(unit, child, unit->parent);
			}

			child->parent = unit->parent;
			unit->parent = child;

			unit->left = child->right;
			child->right = unit;
			if (unit->left) {
				unit->left->parent = unit;
			}
			
			unit->height = get_height(unit);
			child->height = get_height(child);

			if (child->parent == nullptr) {
				root = child;
			}
		}

		void right_rightRotate(value_type *unit) {
			value_type* child = unit->right;

			if (unit->parent) {
				change_parent_child(unit, child, unit->parent);
			}

			child->parent = unit->parent;
			unit->parent = child;

			unit->right = child->left;
			child->left = unit;
			if (unit->right) {
				unit->right->parent = unit;
			}

			unit->height = get_height(unit);
			child->height = get_height(child);

			if (child->parent == nullptr) {
				root = child;
			}
		}

		void left_rightRotate(value_type *unit) {
			right_rightRotate(unit->left);
			left_leftRotate(unit);
		}

		void right_leftRotate(value_type *unit) {
			left_leftRotate(unit->right);
			right_rightRotate(unit);
		}

		void balance_delete(value_type *unit) {
			unit->height = get_height(unit);
			bf_type unit_bf = get_bf(unit);
			bf_type lunit_bf = get_bf(unit->left);
			bf_type runit_bf = get_bf(unit->right);

			if (unit_bf == 2 && lunit_bf == 1) {
				left_leftRotate(unit); 
			} else if (unit_bf == 2 && lunit_bf == -1) {
				left_rightRotate(unit);
			} else if (unit_bf == 2 && lunit_bf == 0) {
				left_leftRotate(unit);
			} else if (unit_bf == -2 && runit_bf == -1) {
				right_rightRotate(unit);
			} else if (unit_bf == -2 && runit_bf == 1) {
				right_leftRotate(unit);
			} else if (unit_bf == -2 && runit_bf == 0) { 
				right_rightRotate(unit);
			}
		}

		void balance_insert(value_type* unit) { 
			while (unit != nullptr) {
				unit->height = get_height(unit);

				bf_type unit_bf = get_bf(unit);
				bf_type lunit_bf = get_bf(unit->left);
				bf_type runit_bf = get_bf(unit->right);

				if (unit_bf == 2 && lunit_bf == 1) {
					left_leftRotate(unit);
				} else if (unit_bf == -2 && runit_bf == -1) {
					right_rightRotate(unit);
				} else if (unit_bf == -2 && runit_bf == 1) {
					right_leftRotate(unit);
				} else if (unit_bf == 2 && lunit_bf == -1) {
					left_rightRotate(unit);
				}
				unit = unit->parent;
			}
		}
		
		value_type *find_node(key_type key) {
			value_type *current = root;
			while (current && current->key != key) {
				if (_compare(key, current->key)) {
					if (!current->left) {
						return current;
					}
					current = current->left;
				} else {
					if (!current->right) {
						return current;
					}
					current = current->right;
				}
			}
			return current;
		}
	};
}