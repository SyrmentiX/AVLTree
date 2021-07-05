#define CATCH_CONFIG_MAIN
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <future>
#include <thread>
#include <ctime>
#include <condition_variable>
#include "catch.hpp"
#include "list.hpp"

using namespace fefu;

TEST_CASE("TEST") {

	/*SECTION("PUSH/ERASE TEST") {\
		std::cout << "PUSH/ERASE TEST" << std::endl;
		List<int> list;
		std::vector<std::thread> threads;
		int threadsAmount = 4;
		int numberOfElements = 100;

		for (int i = 0; i < numberOfElements * threadsAmount; ++i) {
			threads.push_back(std::thread(&List<int>::push_back, &list, i));
		}

		for (int i = 0; i < numberOfElements * threadsAmount; ++i) {
			threads[i].join();
		}

		REQUIRE(list.size() == numberOfElements * threadsAmount);

		for (int i = 0; i < numberOfElements * threadsAmount; ++i) {
			REQUIRE(list.find(i).get() == i);
		}
			
		threads.clear();
		int numberOfDeletes = 20;

		for (int i = 0; i < threadsAmount; ++i) {
			threads.push_back(std::thread([&](int th) {
				for (int j = 0; j < numberOfDeletes; ++j) {
					auto it = list.find(j + th * numberOfDeletes);
					list.erase(it);
				}
				}, i));
		}

		for (int i = 0; i < threadsAmount; ++i) {
			threads[i].join();
		}

		for (int i = numberOfDeletes * threadsAmount; i < list.size(); ++i) {
			REQUIRE(list.find(i).get() == i);
		}

		std::cout << "LIST_SIZE = " << list.size() << std::endl;
		REQUIRE(list.size() == (numberOfElements * threadsAmount) - (numberOfDeletes * threadsAmount));
	}

	SECTION("PUSH/ERASE MEDIUM GRAINED") {
		std::cout << "MULTIPLE PUSH/ERASE TEST" << std::endl;
		List<int> list;
		int count = 0;
		int threadsAmount = 4;
		int numberOfElements = 100;

		std::vector<std::thread> threads;

		auto startThreaded = std::chrono::high_resolution_clock::now();

		for (int i = 0; i < threadsAmount; ++i) {
			threads.push_back(std::thread([&](int th) {
				for (int j = 0; j < numberOfElements * 2; ++j) {
					list.push_back(j + th * numberOfElements);
				}
			}, i));
			threads.push_back(std::thread([&](int th) {
				for (int j = 0; j < numberOfElements; ++j) {
					list.push_front(j + (th + threadsAmount) * numberOfElements);
				}
			}, i));
		}

		for (int k = 0; k < threadsAmount * 2; ++k) {
			threads[k].join();
		}

		threads.clear();
		
		threads.push_back(std::thread([&]() {
			for (int j = 0; j < numberOfElements; ++j) {
				auto it = list.begin();
				list.erase(it);
			}
		}));

		threads[0].join();

		auto endThreaded = std::chrono::high_resolution_clock::now();
		auto timeThreaded = std::chrono::duration_cast<std::chrono::milliseconds>(endThreaded - startThreaded);
		std::cout << (double)timeThreaded.count() / 1000.0 << std::endl;

		REQUIRE(list.size() >= (numberOfElements * threadsAmount));

		std::cout << calls << std::endl;
	}*/

	//SECTION("INSERT TEST") {
	//	std::cout << "INSERT/ITERATE TEST" << std::endl;
	//	List<int> list;
	//	int count = 0;
	//	int threadsAmount = 2;
	//	int numberOfElements = 100;

	//	std::vector<std::thread> threads;

	//	auto startThreaded = std::chrono::high_resolution_clock::now();
	//	
	//	for (int i = 0; i < threadsAmount; ++i) {
	//		threads.push_back(std::thread([&](int th) {
	//			for (int j = 0; j < numberOfElements; ++j) {
	//				auto it = list.begin();
	//				list.insert(it, j + th * numberOfElements);
	//			}
	//			}, i));
	//		threads.push_back(std::thread([&](int th) {
	//			for (int j = 0; j < numberOfElements; ++j) {
	//				list.push_front(j + (th + threadsAmount) * numberOfElements);
	//			}
	//			}, i));
	//	}

	//	auto endThreaded = std::chrono::high_resolution_clock::now();
	//	auto timeThreaded = std::chrono::duration_cast<std::chrono::milliseconds>(endThreaded - startThreaded);
	//	std::cout << (double)timeThreaded.count() / 1000.0 << std::endl;

	//	for (int k = 0; k < threadsAmount * 2; ++k) {
	//		threads[k].join();
	//	}

	//	threads.clear();

	//	for (int i = 0; i < threadsAmount; ++i) {
	//		threads.push_back(std::thread([&](int th) {
	//			auto it = list.begin();
	//			auto last = list.end();
	//			while (it != last) {
	//				++it;
	//			}
	//			}, i));
	//	}

	//	for (int k = 0; k < threadsAmount; ++k) {
	//		threads[k].join();
	//	}
	//}
	

	SECTION("SPEED TEST") {
		std::cout << "SPEED TEST" << std::endl;
		while (true) {
			List<int> list;
			int count = 0;
			int threadsAmount = 2;
			int numberOfElements = 500000;

			std::vector<std::thread> threads;

			auto startThreaded = std::chrono::high_resolution_clock::now();

			for (int i = 0; i < threadsAmount; ++i) {
				threads.push_back(std::thread([&](int th) {
					for (int j = 0; j < numberOfElements; ++j) {
						list.push_back(j + th * numberOfElements);
					}
					}, i));
			}

			for (int k = 0; k < threadsAmount; ++k) {
				threads[k].join();
			}

			REQUIRE(list.size() == threadsAmount * numberOfElements);

			threads.clear();

			for (int i = 0; i < threadsAmount; ++i) {
				threads.push_back(std::thread([&](int th) {
					for (int j = 0; j < numberOfElements / 2; ++j) {
						auto it = list.begin();
						list.erase(it);
					}
					}, i));
			}

			for (int k = 0; k < threadsAmount; ++k) {
				threads[k].join();
			}

			REQUIRE(list.size() >= (size_t)(threadsAmount * (numberOfElements / 2)));

			auto endThreaded = std::chrono::high_resolution_clock::now();
			auto timeThreaded = std::chrono::duration_cast<std::chrono::milliseconds>(endThreaded - startThreaded);
			
			std::cout << (double)timeThreaded.count() / 1000.0 << std::endl;

			std::cout << numberOfElements * threadsAmount - list.size() << std::endl;
			std::cout << calls << std::endl;
			calls = 0;
			//break;
		}
	}
}