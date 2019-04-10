#include <LibCore/CObject.h>
#include <LibCore/CEvent.h>
#include <LibGUI/GEventLoop.h>
#include <AK/Assertions.h>
#include <stdio.h>

CObject::CObject(CObject* parent)
    : m_parent(parent)
{
    if (m_parent)
        m_parent->add_child(*this);
}

CObject::~CObject()
{
    stop_timer();
    if (m_parent)
        m_parent->remove_child(*this);
    auto children_to_delete = move(m_children);
    for (auto* child : children_to_delete)
        delete child;
}

void CObject::event(CEvent& event)
{
    switch (event.type()) {
    case GEvent::Timer:
        return timer_event(static_cast<CTimerEvent&>(event));
    case GEvent::DeferredDestroy:
        delete this;
        break;
    case GEvent::ChildAdded:
    case GEvent::ChildRemoved:
        return child_event(static_cast<CChildEvent&>(event));
    case GEvent::Invalid:
        ASSERT_NOT_REACHED();
        break;
    default:
        break;
    }
}

void CObject::add_child(CObject& object)
{
    m_children.append(&object);
    GEventLoop::current().post_event(*this, make<CChildEvent>(GEvent::ChildAdded, object));
}

void CObject::remove_child(CObject& object)
{
    for (ssize_t i = 0; i < m_children.size(); ++i) {
        if (m_children[i] == &object) {
            m_children.remove(i);
            GEventLoop::current().post_event(*this, make<CChildEvent>(GEvent::ChildRemoved, object));
            return;
        }
    }
}

void CObject::timer_event(CTimerEvent&)
{
}

void CObject::child_event(CChildEvent&)
{
}

void CObject::start_timer(int ms)
{
    if (m_timer_id) {
        dbgprintf("CObject{%p} already has a timer!\n", this);
        ASSERT_NOT_REACHED();
    }

    m_timer_id = GEventLoop::register_timer(*this, ms, true);
}

void CObject::stop_timer()
{
    if (!m_timer_id)
        return;
    bool success = GEventLoop::unregister_timer(m_timer_id);
    ASSERT(success);
    m_timer_id = 0;
}

void CObject::delete_later()
{
    GEventLoop::current().post_event(*this, make<CEvent>(CEvent::DeferredDestroy));
}

void CObject::dump_tree(int indent)
{
    for (int i = 0; i < indent; ++i) {
        printf(" ");
    }
    printf("%s{%p}\n", class_name(), this);

    for (auto* child : children()) {
        child->dump_tree(indent + 2);
    }
}

void CObject::deferred_invoke(Function<void(CObject&)> invokee)
{
    GEventLoop::current().post_event(*this, make<CDeferredInvocationEvent>(move(invokee)));
}