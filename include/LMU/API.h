#pragma once

namespace LMU::API
{
	struct Message
	{
		enum Type : std::uint32_t
		{
			kPostCreateMarkersHookChance
		};
	};

	struct PostCreateMarkersHookChanceMessage : Message
	{
		static constexpr inline Type type = Type::kPostCreateMarkersHookChance;

		void (*PostCreateMarkers)(RE::GFxValue::ObjectInterface* a_objIface, void* a_data, bool a_isDObj) = {};
	};

	template <typename T>
	concept valid_message = std::is_base_of_v<Message, T>;

	template <typename MessageT>
	requires valid_message<MessageT> inline const MessageT* TranslateAs(SKSE::MessagingInterface::Message* a_msg)
	{
		if constexpr (std::is_same_v<Message, MessageT>) {
			return static_cast<Message*>(a_msg->data);
		} else {
			if (a_msg->type == MessageT::type && a_msg->dataLen == sizeof(MessageT)) {
				return static_cast<MessageT*>(a_msg->data);
			}

			return nullptr;
		}
	}
}