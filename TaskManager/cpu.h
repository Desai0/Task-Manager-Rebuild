#pragma once
#include "Taskm.h"
#include <thread>
#include <memory>
// NOTHING WORKS DO NOT PAY ATTENTION
std::shared_ptr<bool> loop;

class Monitor
{
private:
	
	static void update_loop()
	{
		Taskm taskm;

		std::cout << *loop << std::endl;
		while (*loop)
		{
			taskm.update();
			taskm.save_json();
			std::this_thread::sleep_for(std::chrono::microseconds(100));
			std::cout << "Thread loop: " << *loop << std::endl;
		}
	}

	std::thread mainThread;

public:
	Monitor()
	{
		loop = std::make_shared<bool>(true);

		std::thread th(Monitor::update_loop);
		mainThread.swap(th);
	}

	

	void monitor(std::string target)
	{

	}

	void stop_loop()
	{
		loop = std::make_shared<bool>(false);
		mainThread.join();
	}

	~Monitor()
	{
		stop_loop();
	}

};

