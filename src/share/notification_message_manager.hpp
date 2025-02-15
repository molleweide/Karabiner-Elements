#pragma once

#include "json_writer.hpp"
#include "modifier_flag_manager.hpp"
#include "types/device_id.hpp"
#include <map>
#include <pqrs/dispatcher.hpp>
#include <sstream>

// `krbn::notification_message_manager` can be used safely in a multi-threaded environment.

namespace krbn {
class notification_message_manager final : public pqrs::dispatcher::extra::dispatcher_client {
public:
  notification_message_manager(std::weak_ptr<console_user_server_client> weak_console_user_server_client)
      : dispatcher_client(),
        weak_console_user_server_client_(weak_console_user_server_client) {
    enqueue_to_dispatcher([this] {
      save_message("");
    });
  }

  virtual ~notification_message_manager(void) {
    detach_from_dispatcher();
  }

  void async_set_device_ungrabbable_temporarily_message(device_id id,
                                                        const std::string& message) {
    enqueue_to_dispatcher([this, id, message] {
      device_ungrabbable_temporarily_messages_[id] = message;

      save_message_if_needed();
    });
  }

  void async_erase_device(device_id id) {
    enqueue_to_dispatcher([this, id] {
      device_ungrabbable_temporarily_messages_.erase(id);

      save_message_if_needed();
    });
  }

  void async_update_sticky_modifiers_message(const modifier_flag_manager& modifier_flag_manager) {
    //
    // Build message synchronously.
    //

    std::stringstream ss;

    for (const auto& f : {
             modifier_flag::left_control,
             modifier_flag::left_shift,
             modifier_flag::left_option,
             modifier_flag::left_command,
             modifier_flag::right_control,
             modifier_flag::right_shift,
             modifier_flag::right_option,
             modifier_flag::right_command,
             modifier_flag::fn,
         }) {
      if (modifier_flag_manager.is_sticky_active(f)) {
        if (auto name = get_modifier_flag_name(f)) {
          ss << "sticky " << *name << "\n";
        }
      }
    }

    auto message = ss.str();

    //
    // Update sticky_modifiers_message_.
    //

    enqueue_to_dispatcher([this, message] {
      sticky_modifiers_message_ = message;

      save_message_if_needed();
    });
  }

  void async_clear_sticky_modifiers_message(void) {
    enqueue_to_dispatcher([this] {
      sticky_modifiers_message_ = "";

      save_message_if_needed();
    });
  }

private:
  void save_message(const std::string& message) const {
    json_writer::async_save_to_file(
        nlohmann::json::object({
            {"body", message},
        }),
        constants::get_notification_message_file_path(),
        0755,
        0644);
  }

  void save_message_if_needed(void) {
    auto message = make_message();
    if (previous_message_ != message) {
      previous_message_ = message;
      save_message(message);
    }
  }

  std::string make_message(void) const {
    std::stringstream ss;

    for (const auto& m : device_ungrabbable_temporarily_messages_) {
      if (!m.second.empty()) {
        ss << m.second;
        break;
      }
    }

    if (!sticky_modifiers_message_.empty()) {
      if (ss.tellp() > 0) {
        ss << "\n";
      }
      ss << sticky_modifiers_message_;
    }

    return ss.str();
  }

  std::weak_ptr<console_user_server_client> weak_console_user_server_client_;
  std::map<device_id, std::string> device_ungrabbable_temporarily_messages_;
  std::string sticky_modifiers_message_;
  std::string previous_message_;
};
} // namespace krbn
