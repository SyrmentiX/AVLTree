#define CATCH_CONFIG_MAIN
#include <iostream>
#include "catch.hpp"
#include "avl.hpp"

using namespace fefu;

TEST_CASE("TEST") {
	SECTION("INSERT TEST") {
		AVLTree<int> tree({ 5, 10, 15, 20, 25, 30, 35 });
		tree.insert(40);
		tree.insert(45);
		tree.insert(50);
		tree.insert(55);
		REQUIRE(tree.size() == 11);
		auto it = tree.begin();
		REQUIRE(*tree.find(5) == *it);
		++it;
		REQUIRE(*it == 10);
		++it;
		REQUIRE(*it == 15);
		++it;
		REQUIRE(*it == 20);
		++it;
		REQUIRE(*it == 25);
		++it;
		REQUIRE(*it == 30);
		++it;
		REQUIRE(*it == 35);
		++it;
		REQUIRE(*it == 40);
		++it;
		REQUIRE(*it == 45);
		++it;
		REQUIRE(*it == 50);
		++it;
		REQUIRE(*it == 55);
		++it;
		REQUIRE(*it == *tree.end());
	}
	SECTION("DELETE TEST") {
		AVLTree<int> tree({ 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55 });
		if (true) {
			auto it1 = tree.find(20);
			tree.erase(20);
			tree.erase(25);
			tree.erase(30);
			++it1;
		}
		tree.insert(20);
		tree.insert(25);
		tree.insert(30);

		auto it2 = tree.find(40);
		tree.erase(40);
		for (int i = 0; i < 10; ++i) {
			++it2;
		}
		REQUIRE(*it2 == *tree.end());

		if (true) {
			auto it1 = tree.find(20);
			tree.erase(20);
			tree.erase(25);
			tree.erase(30);
			tree.erase(5);
			tree.erase(10);
			tree.erase(15);
			tree.erase(35);
			tree.erase(45);
			tree.erase(50);
			tree.erase(55);
			REQUIRE(tree.empty());
			++it1;
		}

		tree.insert({ 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55 });
		it2 = tree.find(20);
		tree.clear();
	}

	SECTION("CONSISTENCY") {
		int n = 5000;
		srand(time(0));
		AVLTree<int> tree;

		for (int i = 0; i < n; ++i) {
			int value = rand() % n;
			tree.insert(value);
		}

		std::vector<avl_iterator<int>> its;

		for (int i = 0; i < n; ++i) {
			avl_iterator<int> iter = tree.begin();
			int m = std::rand() % (n / 2);

			for (int j = 1; j < m; ++j) {
				iter++;
				if (iter == tree.end()) break;
			}

			its.push_back(iter);
		}

		for (int i = 0; i < n; ++i) {
			int value = std::rand() % n;
			tree.erase(value);
		}

		for (auto it : its) {
			while (it != tree.end()) {
				it++;
			}
		}
	}
}