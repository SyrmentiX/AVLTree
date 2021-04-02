#define CATCH_CONFIG_MAIN
#include <iostream>
#include "catch.hpp"
#include "avl.hpp"

using namespace fefu;

TEST_CASE("TEST") {
	SECTION("INSERT TEST") {
		AVLTree<int, int> tree({ {5,5}, {10,10}, {15,15}, {20,20}, {25,25}, {30,30}, {35,35} });
		tree.insert(40, 40);
		tree.insert(45, 45);
		tree.insert(50, 50);
		tree.insert({55, 55});
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
		AVLTree<int, int> tree({ {5,5}, {10,10}, {15,15}, {20,20}, {25,25}, {30,30}, {35,35}, {40,40}, {45,45}, {50,50}, {55,55} });
		if (true) {
			auto it1 = tree.find(20);
			tree.erase(20);
			tree.erase(25);
			tree.erase(30);
			++it1;
		}
		tree.insert(20, 20);
		tree.insert(25, 25);
		tree.insert(30, 30);

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

		tree.insert({ {5,5}, {10,10}, {15,15}, {20,20}, {25,25}, {30,30}, {35,35}, {40,40}, {45,45}, {50,50}, {55,55} });
		it2 = tree.find(20);
		tree.clear();
	}

	SECTION("CONSISTENCY") {
		AVLTree<int, int> tree;
		AVLIterator<int, int> iter;
		tree.insert(std::pair<int, int>(2, 1));
		tree.insert(std::pair<int, int>(4, 3));
		tree.insert(std::pair<int, int>(6, 5));

		iter = tree.begin();
		iter++;

		REQUIRE(*iter == 4);

		tree.erase(3);

		iter++;

		REQUIRE(*iter == 6);
	}

	SECTION("RANDOM CONSISTENCY") {
		int n = 5000;
		srand(time(0));
		AVLTree<int, int> tree;

		for (int i = 0; i < n; ++i) {
			int value = rand() % n;
			tree.insert(value, i);
		}

		std::vector<AVLIterator<int, int>> its;

		for (int i = 0; i < n; ++i) {
			AVLIterator<int, int> iter = tree.begin();
			int m = std::rand() % (n / 2);

			for (int j = 1; j < m; ++j) {
				iter++;
				if (iter == tree.end()) break;
			}

			its.push_back(iter);
		}

		for (int i = 0; i < n; ++i) {
			int value = std::rand() % n;
			tree.erase(i);
		}

		for (auto it : its) {
			while (it != tree.end()) {
				it++;
			}
		}
	}
}