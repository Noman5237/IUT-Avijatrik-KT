#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "std_msgs/msg/int16.hpp"

#include "temperature_control_systems_interfaces/srv/get_current_temperature.hpp"
#include "temperature_control_systems_interfaces/srv/increment_decrement_temperature.hpp"
#include "temperature_control_systems_interfaces/action/set_temperature.hpp"

using namespace std;
using namespace temperature_control_systems_interfaces::srv;
using namespace temperature_control_systems_interfaces::action;

class TemperatureSystemsControllerNode : public rclcpp::Node {
private:
	// context prefix
	const string context_prefix = "temperature_control_systems";
	// current temperature
	short current_temperature;
	// timer to monitor the temperature
	rclcpp::TimerBase::SharedPtr temperature_monitor_timer;
	// publisher to publish the temperature
	rclcpp::Publisher<std_msgs::msg::Int16>::SharedPtr temperature_publisher;
	// service that returns the current temperature
	rclcpp::Service<GetCurrentTemperature>::SharedPtr get_current_temperature_service;
	// service that increments or decrements the current temperature
	rclcpp::Service<IncrementDecrementTemperature>::SharedPtr increment_decrement_temperature_service;
	// action server to set the temperature
	rclcpp_action::Server<SetTemperature>::SharedPtr set_temperature_action_server;
	// status of the action server busy or not
	bool action_server_busy = false;


public:
#pragma clang diagnostic push
#pragma ide diagnostic ignored "UnusedValue"
	
	
	TemperatureSystemsControllerNode() : Node("temperature_control_systems") {
		// initialize the current temperature with random value between 15 and 100
		current_temperature = rand() % 85 + 15;
		// create a publisher
		temperature_publisher = this->create_publisher<std_msgs::msg::Int16>(context_prefix + "__temperature", 10);
		// create a monitor timer
		temperature_monitor_timer = this->create_wall_timer(1s, [this] { temperature_monitor_callback(); });
		// create a service to return the current temperature
		get_current_temperature_service = this->create_service<GetCurrentTemperature>(
				context_prefix + "__get_current_temperature",
				[this](const std::shared_ptr<GetCurrentTemperature::Request> request,
				       const std::shared_ptr<GetCurrentTemperature::Response> response) {
					get_current_temperature_callback(request, response);
				}
		);
		// create a service to increment or decrement the current temperature
		increment_decrement_temperature_service = this->create_service<IncrementDecrementTemperature>(
				context_prefix + "__increment_decrement_temperature",
				[this](const std::shared_ptr<IncrementDecrementTemperature::Request> request,
				       const std::shared_ptr<IncrementDecrementTemperature::Response> response) {
					increment_decrement_temperature_callback(request, response);
				}
		);
		
		// create an action server to set the temperature
		set_temperature_action_server = rclcpp_action::create_server<SetTemperature>(
				this,
				context_prefix + "__set_temperature",
				[this](const rclcpp_action::GoalUUID &uuid,
				       const shared_ptr<const SetTemperature::Goal> goal) {
					return handle_set_temperature_action_callback(uuid, goal);
				},
				[this](const shared_ptr<rclcpp_action::ServerGoalHandle<SetTemperature>> goalHandle) {
					return cancel_set_temperature_action_callback(goalHandle);
				},
				[this](const shared_ptr<rclcpp_action::ServerGoalHandle<SetTemperature>> goalHandle) {
					accepted_set_temperature_action_callback(goalHandle);
				}
		);
		
	}

#pragma clang diagnostic pop

private:
	void
	temperature_monitor_callback() {
		// publish the current temperature
		auto message = std_msgs::msg::Int16();
		message.data = current_temperature;
		RCLCPP_DEBUG(this->get_logger(), "Publishing: '%s'", to_string(message.data).c_str());
		temperature_publisher->publish(message);
	}

private:
	void
	get_current_temperature_callback(
			const std::shared_ptr<GetCurrentTemperature::Request> request,
			const std::shared_ptr<GetCurrentTemperature::Response> response) {
		RCLCPP_INFO(this->get_logger(), "Incoming request for current temperature");
		response->temperature = current_temperature;
	}

private:
	void
	increment_decrement_temperature_callback(
			const std::shared_ptr<IncrementDecrementTemperature::Request> request,
			const std::shared_ptr<IncrementDecrementTemperature::Response> response) {
		bool is_increment = request->increment;
		RCLCPP_INFO(this->get_logger(), "Incoming request for %s temperature",
		            is_increment ? "increment" : "decrement");
		// check if the temperature can be increased
		bool can_increase_temperature = rand() % 2;
		if (!can_increase_temperature) {
			response->success = false;
			response->temperature = current_temperature;
			response->message = "Temperature cannot be " + string(is_increment ? "increased" : "decreased");
			return;
		}
		// sleep for a random time between 0.1s and 0.5s
		this_thread::sleep_for(chrono::milliseconds(rand() % 400 + 100));
		// increase/decrease the temperature
		current_temperature += is_increment ? 1 : -1;
		response->success = true;
		response->temperature = current_temperature;
		response->message = "Temperature " + string(is_increment ? "increased" : "decreased") + " successfully";
	}

private:
	rclcpp_action::GoalResponse
	handle_set_temperature_action_callback(const rclcpp_action::GoalUUID &uuid,
	                                       const shared_ptr<const SetTemperature::Goal> goal) {
		RCLCPP_INFO(this->get_logger(), "Incoming request for setting temperature");
		if (action_server_busy) {
			return rclcpp_action::GoalResponse::REJECT;
		}
		action_server_busy = true;
		return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
	}

private:
	rclcpp_action::CancelResponse
	cancel_set_temperature_action_callback(
			const shared_ptr<rclcpp_action::ServerGoalHandle<SetTemperature>> goalHandle) {
		RCLCPP_INFO(this->get_logger(), "Incoming request for cancelling setting temperature");
		return rclcpp_action::CancelResponse::ACCEPT;
	}

private:
	void
	accepted_set_temperature_action_callback(
			const shared_ptr<rclcpp_action::ServerGoalHandle<SetTemperature>> goalHandle) {
		RCLCPP_INFO(this->get_logger(), "Incoming request for accepting setting _current_temperature");
		auto feedback = std::make_shared<SetTemperature::Feedback>();
		auto result = std::make_shared<SetTemperature::Result>();
		auto goal = goalHandle->get_goal();
		auto target_temperature = goal->temperature;
		auto initial_temperature = current_temperature;
		auto _current_temperature = current_temperature;
		auto is_increment = target_temperature > current_temperature;
		
		while (_current_temperature != target_temperature) {
			// check if there is a cancel request
			if (goalHandle->is_canceling()) {
//				result->success = false;
				result->temperature = _current_temperature;
//				result->message = "Setting _current_temperature cancelled";
				goalHandle->canceled(result);
				action_server_busy = false;
				return;
			}
			
			// call the increment/decrement service
			auto request = std::make_shared<IncrementDecrementTemperature::Request>();
			request->increment = is_increment;
			// create a client for the service
			auto increment_decrement_temperature_client = this->create_client<IncrementDecrementTemperature>(
					context_prefix + "__increment_decrement_temperature");
			// send the request
			auto future = increment_decrement_temperature_client->async_send_request(request);
			// wait for the response indefinitely
			auto response = future.get();
			// publish the feedback
			_current_temperature = response->temperature;
			feedback->temperature = _current_temperature;
			feedback->progress = (_current_temperature - initial_temperature) * 100 /
			                     (target_temperature - initial_temperature);
			goalHandle->publish_feedback(feedback);
			RCLCPP_DEBUG(this->get_logger(), "Publishing feedback: '%s'", to_string(feedback->progress).c_str());
		}

//		result->success = true;
		result->temperature = _current_temperature;
//		result->message = "Temperature set successfully";
		goalHandle->succeed(result);
		action_server_busy = false;
	}
};

int
main(int argc, char *argv[]) {
	rclcpp::init(argc, argv);
	rclcpp::spin(std::make_shared<TemperatureSystemsControllerNode>());
	rclcpp::shutdown();
	return 0;
}