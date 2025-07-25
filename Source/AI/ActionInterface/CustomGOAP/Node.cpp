

#include "Node.h"

namespace Divide::goap
{

    I32 Node::last_id_ = 0;

Node::Node() noexcept :  parent_id_(-1), g_(0), h_(0), action_(nullptr)
{
    id_ = ++last_id_;
}

Node::Node(const WorldState& state, I32 g, I32 h, I32 parent_id, const Action* action)
    : ws_(state)
    , parent_id_(parent_id)
    , g_(g)
    , h_(h)
    , action_(action)
{
    id_ = ++last_id_;
}

bool operator<(const Node& lhs, const Node& rhs) noexcept {
    return lhs.f() < rhs.f();
}

//bool Node::operator<(const Node& other) {
//    return f() < other.f();
//}

string Node::toString() const
{
    return Util::StringFormat("Node { id: {} parent: {} F: {} G: {} H: {}, {}\n", id_, parent_id_, f(), g_, h_, ws_.toString());
}

} //namespace Divide::goap
