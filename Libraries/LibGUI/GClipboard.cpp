#include <LibC/SharedBuffer.h>
#include <LibGUI/GClipboard.h>
#include <LibGUI/GEventLoop.h>
#include <WindowServer/WSAPITypes.h>

GClipboard& GClipboard::the()
{
    static GClipboard* s_the;
    if (!s_the)
        s_the = new GClipboard;
    return *s_the;
}

GClipboard::GClipboard()
{
}

GClipboard::DataAndType GClipboard::data_and_type() const
{
    WSAPI_ClientMessage request;
    request.type = WSAPI_ClientMessage::Type::GetClipboardContents;
    auto response = GWindowServerConnection::the().sync_request(request, WSAPI_ServerMessage::Type::DidGetClipboardContents);
    if (response.clipboard.shared_buffer_id < 0)
        return {};
    auto shared_buffer = SharedBuffer::create_from_shared_buffer_id(response.clipboard.shared_buffer_id);
    if (!shared_buffer) {
        dbgprintf("GClipboard::data() failed to attach to the shared buffer\n");
        return {};
    }
    if (response.clipboard.contents_size > shared_buffer->size()) {
        dbgprintf("GClipboard::data() clipping contents size is greater than shared buffer size\n");
        return {};
    }
    auto data = String((const char*)shared_buffer->data(), response.clipboard.contents_size);
    auto type = String(response.text, response.text_length);
    return { data, type };
}

void GClipboard::set_data(const StringView& data, const String& type)
{
    WSAPI_ClientMessage request;
    request.type = WSAPI_ClientMessage::Type::SetClipboardContents;
    auto shared_buffer = SharedBuffer::create_with_size(data.length() + 1);
    if (!shared_buffer) {
        dbgprintf("GClipboard::set_data() failed to create a shared buffer\n");
        return;
    }
    if (!data.is_empty())
        memcpy(shared_buffer->data(), data.characters_without_null_termination(), data.length() + 1);
    else
        ((u8*)shared_buffer->data())[0] = '\0';
    shared_buffer->seal();
    shared_buffer->share_with(GWindowServerConnection::the().server_pid());
    request.clipboard.shared_buffer_id = shared_buffer->shared_buffer_id();
    request.clipboard.contents_size = data.length();

    ASSERT(type.length() < (ssize_t)sizeof(request.text));
    if (!type.is_null())
        strcpy(request.text, type.characters());
    request.text_length = type.length();

    auto response = GWindowServerConnection::the().sync_request(request, WSAPI_ServerMessage::Type::DidSetClipboardContents);
    ASSERT(response.clipboard.shared_buffer_id == shared_buffer->shared_buffer_id());
}

void GClipboard::did_receive_clipboard_contents_changed(Badge<GWindowServerConnection>, const String& data_type)
{
    if (on_content_change)
        on_content_change(data_type);
}
