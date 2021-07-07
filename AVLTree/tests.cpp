#define CATCH_CONFIG_MAIN
#include <iostream>
#include <future>
#include <thread>
#include <ctime>
#include <condition_variable>
#include "catch.hpp"
#include "list.hpp"

using namespace fefu;

TEST_CASE("TEST") {
	SECTION("LOCK TEST") {
		rw_lock lock;
		int a = 10;
		auto th1 = std::thread([&](int th) {
			lock.wlock();
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			a = 15;
			lock.unlock();
			}, 0);

		auto th2 = std::thread([&](int th) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			lock.rlock();
			a = 5;
			lock.unlock();
			}, 0);

		th1.join();
		th2.join();

		REQUIRE(a == 5);
	}

	SECTION("PUSH_BACK/INSERT/PUSH_FRONT") {
		std::cout << "PUSH_BACK/INSERT/PUSH_FRONT" << std::endl;
		List<int> list({1, 2, 3, 4});
		list.push_back(5);
		auto it = list.end();
		--it;

		REQUIRE(list.size() == 5);
		REQUIRE(it.get() == 5);
		REQUIRE(*it == 5);
		it = list.begin();

		for (int i = 1; i < 6; ++i) {
			REQUIRE(*it == i);
			++it;
		}

		list.push_front(0);
		it = list.end();
		list.insert(it, 6);
		--it;
		list.insert(it, 7);
		REQUIRE(*list.begin() == 0);
		it = --list.end();
		REQUIRE(*it == 7);
		REQUIRE(list.size() == 8);
	}

	SECTION("ERASE/POP_BACK") {
		std::cout << "ERASE/POP_BACK" << std::endl;
		List<int> list({ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 });
		list.pop_back();
		auto it = list.end();
		--it;
		REQUIRE(*it == 9);
		REQUIRE(list.size() == 9);
		list.erase(it);
		--it;
		REQUIRE(*it == 8);
		REQUIRE(list.size() == 8);
	}

	SECTION("== && !=") {
		std::cout << "== && !=" << std::endl;
		List<int> list({ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 });
		auto it1 = list.begin();
		auto it2 = list.end();
		REQUIRE(bool(it1 != it2));
		auto it3 = list.begin();
		REQUIRE(bool(it1 == it3));
	}

	SECTION("PUSH/ERASE TEST") {
		std::cout << "PUSH/ERASE TEST" << std::endl;
		List<int> list;
		std::vector<std::thread> threads;
		int threadsAmount = 2;
		int numberOfElements = 100;

		for (int i = 0; i < numberOfElements * threadsAmount; ++i) {
			threads.push_back(std::thread(&List<int>::push_back, &list, i));
		}

		for (int i = 0; i < numberOfElements * threadsAmount; ++i) {
			threads[i].join();
		}

		REQUIRE(list.size() == static_cast<size_t>(numberOfElements * threadsAmount));
		
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

		for (size_t i = numberOfDeletes * threadsAmount; i < list.size(); ++i) {
			REQUIRE(list.find(i).get() == i);
		}
	}

	SECTION("MULTIPLE PUSH/ERASE") {
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

		REQUIRE(list.size() >= static_cast<size_t>(numberOfElements * threadsAmount));
	}

	SECTION("INSERT/ITERATE TEST") {
		std::cout << "INSERT/ITERATE TEST" << std::endl;
		List<int> list;
		int count = 0;
		int threadsAmount = 2;
		int numberOfElements = 100;

		std::vector<std::thread> threads;

		auto startThreaded = std::chrono::high_resolution_clock::now();
		
		for (int i = 0; i < threadsAmount; ++i) {
			threads.push_back(std::thread([&](int th) {
				for (int j = 0; j < numberOfElements; ++j) {
					auto it = list.begin();
					list.insert(it, j + th * numberOfElements);
				}
				}, i));
			threads.push_back(std::thread([&](int th) {
				for (int j = 0; j < numberOfElements; ++j) {
					list.push_front(j + (th + threadsAmount) * numberOfElements);
				}
				}, i));
		}

		auto endThreaded = std::chrono::high_resolution_clock::now();
		auto timeThreaded = std::chrono::duration_cast<std::chrono::milliseconds>(endThreaded - startThreaded);
		std::cout << (double)timeThreaded.count() / 1000.0 << std::endl;

		for (int k = 0; k < threadsAmount * 2; ++k) {
			threads[k].join();
		}

		threads.clear();

		for (int i = 0; i < threadsAmount; ++i) {
			threads.push_back(std::thread([&](int th) {
				auto it = list.begin();
				auto last = list.end();
				while (it != last) {
					++it;
				}
				}, i));
		}

		for (int k = 0; k < threadsAmount; ++k) {
			threads[k].join();
		}
	}

	SECTION("ERASE TEST") {
		std::cout << "ERASE TEST" << std::endl;
		for (int threadsAmount = 1; threadsAmount <= 4; threadsAmount *= 2) {
			List<int> list;
			int count = 0;
			int numberOfElements = 1000000;

			std::vector<std::thread> threads;

			auto startThreaded = std::chrono::high_resolution_clock::now();

			for (int i = 0; i < threadsAmount; ++i) {
				threads.push_back(std::thread([&](int th) {
					for (int j = 0; j < numberOfElements / threadsAmount; ++j) {
						list.push_back(j + th * numberOfElements / threadsAmount);
					}
					}, i));
			}

			for (int k = 0; k < threadsAmount; ++k) {
				threads[k].join();
			}

			REQUIRE(list.size() == numberOfElements);

			threads.clear();
			int numberOfDeletions = numberOfElements / 2;

			for (int i = 0; i < threadsAmount; ++i) {
				threads.push_back(std::thread([&](int th) {
					for (int j = 0; j < numberOfDeletions / threadsAmount; ++j) {
						auto it = list.begin();
						list.erase(it);
					}
					}, i));
			}

			for (int k = 0; k < threadsAmount; ++k) {
				threads[k].join();
			}

			REQUIRE(list.size() >= static_cast<size_t>(numberOfElements - numberOfDeletions));

			auto endThreaded = std::chrono::high_resolution_clock::now();
			auto timeThreaded = std::chrono::duration_cast<std::chrono::milliseconds>(endThreaded - startThreaded);
			
			//std::cout << "NUMBER OF THREADS = " << threadsAmount << std::endl;
			//std::cout << "TIME = " << (double)timeThreaded.count() / 1000.0 << std::endl;
		}
	}

	SECTION("ITERATION/ERASE TEST") {
		std::cout << "ITERATION/ERASE TEST" << std::endl;
		srand(time(nullptr));
		List<int> list;
		int count = 0;
		int threadsAmount = 4;
		int numberOfElements = 1000000;

		std::vector<std::thread> threads;

		auto startThreaded = std::chrono::high_resolution_clock::now();

		for (int i = 0; i < threadsAmount; ++i) {
			threads.push_back(std::thread([&](int th) {
				for (int j = 0; j < numberOfElements / threadsAmount; ++j) {
					list.push_back(j + th * numberOfElements / threadsAmount);
				}
				}, i));
		}

		for (int k = 0; k < threadsAmount; ++k) {
			threads[k].join();
		}

		REQUIRE(list.size() == numberOfElements);
		threads.clear();

		std::condition_variable cv;

		int numberOfDeletions = numberOfElements / 2;

		for (int i = 0; i < threadsAmount / 2; ++i) {
			threads.push_back(std::thread([&](int th) {
				{
					std::mutex mutex_;
					std::unique_lock<std::mutex> cvlock(mutex_);
					cv.wait(cvlock);
				}
				for (int j = 0; j < numberOfDeletions / threadsAmount / 2; ++j) {
					auto it = list.begin();
					list.erase(it);
				}
			}, i));
			threads.push_back(std::thread([&](int th) {
				{
					std::mutex mutex_;
					std::unique_lock<std::mutex> cvlock(mutex_);
					cv.wait(cvlock);
				}
				auto it = list.begin();
				int numberOfIterations = rand() % numberOfElements;
				for (int j = 0; j < numberOfIterations; ++j) {
					++it;
				}
			}, i));
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		cv.notify_all();

		for (int k = 0; k < threadsAmount; ++k) {
			threads[k].join();
		}

		auto endThreaded = std::chrono::high_resolution_clock::now();
		auto timeThreaded = std::chrono::duration_cast<std::chrono::milliseconds>(endThreaded - startThreaded);

		//std::cout << "NUMBER OF THREADS = " << threadsAmount << std::endl;
		//std::cout << "TIME = " << (double)timeThreaded.count() / 1000.0 << std::endl;
		REQUIRE(list.size() >= static_cast<size_t>(numberOfElements - numberOfDeletions));
	}

	SECTION("SPEED TEST") {
		std::cout << "SPEED TEST" << std::endl;
		for (int threadsAmount = 1; threadsAmount <= 4; threadsAmount *= 2) {
			List<int> list;
			int count = 0;
			int numberOfElements = 1000000;

			std::vector<std::thread> threads;


			for (int i = 0; i < threadsAmount; ++i) {
				threads.push_back(std::thread([&](int th) {
					for (int j = 0; j < numberOfElements / threadsAmount; ++j) {
						list.push_back(j + th * numberOfElements / threadsAmount);
					}
					}, i));
			}

			for (int k = 0; k < threadsAmount; ++k) {
				threads[k].join();
			}

			REQUIRE(list.size() == numberOfElements);

			std::condition_variable cv;

			threads.clear();
			int numberOfIterations = (numberOfElements / 2) / threadsAmount;
			auto startThreaded = std::chrono::high_resolution_clock::now();

			for (int i = 0; i < threadsAmount; ++i) {
				threads.push_back(std::thread([&](int th) {
					auto it = list.begin();
					for (int j = 0; j < numberOfIterations; ++j) {
						++it;
					}
					std::mutex mutex_;
					std::unique_lock<std::mutex> lck(mutex_);
					cv.wait(lck);
					for (int j = 0; j < numberOfIterations; ++j) {
						list.erase(it);
						++it;
					}
					}, i));
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			cv.notify_all();

			for (int k = 0; k < threadsAmount; ++k) {
				threads[k].join();
			}

			auto endThreaded = std::chrono::high_resolution_clock::now();
			auto timeThreaded = std::chrono::duration_cast<std::chrono::milliseconds>(endThreaded - startThreaded);

			std::cout << "NUMBER OF THREADS = " << threadsAmount << std::endl;
			std::cout << "TIME = " << static_cast<double>(timeThreaded.count() / 1000.0) - 1.0 << std::endl;
		}
	}
}