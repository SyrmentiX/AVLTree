#define CATCH_CONFIG_MAIN

#include <iostream>
#include "catch.hpp"
#include "avl.hpp"

using namespace fefu;

TEST_CASE("TEST") {
	SECTION("INSERT TEST") {
		AVLTree<int, int> tree;
		std::vector<std::thread> threads;
		for (int i = 0; i < 10; ++i) {
			threads.push_back(std::thread([&](int th) {
				tree.insert({ th , th});
			}, i));
		}

		for (int i = 0; i < 10; ++i) {
			threads[i].join();
		}

		REQUIRE(tree.size() == 10);

		for (int i = 0; i < 10; ++i) {
			REQUIRE(*tree.find(i) == i);
		}
	}

	SECTION("DEADLOCK TEST") {
		AVLTree<int, int> tree;
		int count = 0;
		int threadsAmount = 4;
		int numberOfElements = 100;

		std::vector<std::thread> threads;

		for (int i = 0; i < threadsAmount; ++i) {
			threads.push_back(std::thread([&](int th) {
				for (int j = 0; j < numberOfElements; ++j) {
					tree.insert({ j + th * numberOfElements, j + th * numberOfElements });
				}
				}, i));
		}

		for (int k = 0; k < threadsAmount; ++k) {
			threads[k].join();
		}

		REQUIRE(tree.size() == (numberOfElements * threadsAmount));

		for (int i = 0; i < threadsAmount * numberOfElements; ++i) {
			REQUIRE(*tree.find(i) == i);
		}
	}

	SECTION("ITERATION TEST") {
		AVLTree <int, int> tree;
		int count = 0;
		int threadsAmount = 4;
		int numberOfElements = 10;

		std::vector<std::thread> threads;

		for (int i = 0; i < threadsAmount; ++i) {
			threads.push_back(std::thread([&](int th) {
				for (int j = 0; j < numberOfElements; ++j) {
					tree.insert({ j + th * numberOfElements, j + th * numberOfElements });
				}
				}, i));
		}

		for (int k = 0; k < threadsAmount; ++k) {
			threads[k].join();
		}

		threads.clear();

		REQUIRE(tree.size() == (numberOfElements * threadsAmount));

		for (int i = 0; i < threadsAmount * numberOfElements; ++i) {
			REQUIRE(*tree.find(i) == i);
		}

		for (int i = 0; i < threadsAmount; ++i) {
			threads.push_back(std::thread([&]() {
				auto it = tree.begin();
				auto last = tree.end();
				while (it != last) {
					++it;
				}
				}));
		}

		for (int i = 0; i < threadsAmount; ++i) {
			threads[i].join();
		}
	}

	SECTION("ITERATION-INSERT TEST") {
		AVLTree<int, int> tree;
		int count = 0;
		int threadsAmount = 4;
		int numberOfElements = 10;

		std::vector<std::thread> threads;

		for (int i = 0; i < threadsAmount; ++i) {
			threads.push_back(std::thread([&](int th) {
				for (int j = 0; j < numberOfElements; ++j) {
					tree.insert({ j + th * numberOfElements, j + th * numberOfElements });
				}
				}, i));
		}

		for (int k = 0; k < threadsAmount; ++k) {
			threads[k].join();
		}

		threads.clear();

		REQUIRE(tree.size() == (numberOfElements * threadsAmount));

		for (int i = 0; i < threadsAmount * numberOfElements; ++i) {
			REQUIRE(*tree.find(i) == i);
		}

		for (int i = 0; i < threadsAmount; ++i) {
			if (i % 2 == 0) {
				threads.push_back(std::thread([&]() {
					auto it = tree.begin();
					auto last = tree.end();
					while (it != last) {
						++it;
					}
					}));
			}
			else {
				threads.push_back(std::thread([&]() {
					for (int k = 0; k < numberOfElements; ++k) {
						tree.insert({ numberOfElements * 16 + k, numberOfElements * 16 + k });
					}
					}));
			}
		}

		for (int i = 0; i < threadsAmount; ++i) {
			threads[i].join();
		}

		for (int i = 0; i < numberOfElements; ++i) {
			REQUIRE(*tree.find(numberOfElements * 16 + i) == numberOfElements * 16 + i);
		}
	}

	SECTION("INSERT-ERASE TEST") {
		AVLTree<int, int> tree;
		int threadsAmount = 4;
		int numberOfElements = 10;

		std::vector<std::thread> threads;

		for (int i = 0; i < threadsAmount; ++i) {
			threads.push_back(std::thread([&](int th) {
				for (int j = 0; j < numberOfElements; ++j) {
					tree.insert({ j + th * numberOfElements, j + th * numberOfElements });
				}
				}, i));
		}

		for (int k = 0; k < threadsAmount; ++k) {
			threads[k].join();
		}

		threads.clear();

		int count = 0;
		for (int i = 0; i < threadsAmount; ++i) {
			if (i % 2 == 0) {
				threads.push_back(std::thread([&](int th) {
					for (int j = 0; j < numberOfElements; ++j) {
						tree.erase(j + th * numberOfElements);
					}
					}, i));
				++count;
			}
			else {
				threads.push_back(std::thread([&]() {
					for (int k = 0; k < numberOfElements; ++k) {
						tree.insert({ numberOfElements * 16 + k, numberOfElements * 16 + k });
					}
					}));
			}
		}

		for (int i = 0; i < threadsAmount; ++i) {
			threads[i].join();
		}

		REQUIRE(tree.size() == numberOfElements * (threadsAmount + 1 - count));

		for (int i = 0; i < numberOfElements; ++i) {
			REQUIRE(*tree.find(numberOfElements * 16 + i) == numberOfElements * 16 + i);
		}
	}

	SECTION("BACK_FRONT_INSERT_ERASE_POP") {
		AVLTree<int, int> tree;
		int threadsAmount = 4;
		int numberOfElements = 10;
		int expansion = 100;

		std::vector<std::thread> threads;

		for (int i = 0; i < threadsAmount; ++i) {
			threads.push_back(std::thread([&](int th) {
				for (int j = 0; j < numberOfElements; ++j) {
					tree.insert({ j + th * numberOfElements, j + th * numberOfElements });
				}
				}, i));
		}

		for (int k = 0; k < threadsAmount; ++k) {
			threads[k].join();
		}

		threads.clear();

		std::condition_variable cv;

		for (int i = 0; i < threadsAmount; ++i) {
			threads.push_back(std::thread([&](int th) {
				{
					std::mutex mt;
					std::unique_lock<std::mutex> cvlock(mt);
					cv.wait(cvlock);
				}
				for (int j = 0; j < numberOfElements; ++j) {
					int number = th * numberOfElements + (rand() % numberOfElements);
					auto it = tree.find(number);
					switch (j % 4)
					{
					case 0:
						tree.erase(number);
						break;
					case 1:
						tree.insert({ expansion * number, expansion * number });
						break;
					case 2:
						tree.find(expansion * number);
						break;
					case 3:
						++it;
						break;
					default:
						break;
					}
				}
			}, i));
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		cv.notify_all();

		for (int k = 0; k < threadsAmount; ++k) {
			threads[k].join();
		}
	}
}