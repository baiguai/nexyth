#include "node.hpp"

namespace nexyth {

NodePtr makePaneNode(std::shared_ptr<Pane> pane) {
  auto node = std::make_shared<Node>();
  node->type = Node::Type::Leaf;
  node->pane = std::move(pane);
  return node;
}

NodePtr makeSplitNode(NodePtr first, NodePtr second, SplitKind kind,
                      double ratio) {
  auto node = std::make_shared<Node>();
  node->type = Node::Type::Split;
  node->kind = kind;
  node->ratio = ratio;
  node->first = std::move(first);
  node->second = std::move(second);
  return node;
}

NodePtr findPane(const NodePtr& node, int paneId) {
  if (!node) return nullptr;
  if (node->type == Node::Type::Leaf) {
    return node->pane && node->pane->id() == paneId ? node : nullptr;
  }
  if (auto found = findPane(node->first, paneId)) return found;
  return findPane(node->second, paneId);
}

NodePtr findParent(const NodePtr& node, int paneId) {
  if (!node || node->type != Node::Type::Split) return nullptr;
  if ((node->first && node->first->type == Node::Type::Leaf &&
       node->first->pane && node->first->pane->id() == paneId) ||
      (node->second && node->second->type == Node::Type::Leaf &&
       node->second->pane && node->second->pane->id() == paneId)) {
    return node;
  }
  if (auto found = findParent(node->first, paneId)) return found;
  return findParent(node->second, paneId);
}

NodePtr findResizeSplit(const NodePtr& node, int paneId, SplitKind kind) {
  if (!node || node->type != Node::Type::Split) return nullptr;
  const NodePtr child =
      findPane(node->first, paneId) ? node->first : node->second;
  if (!child || !findPane(child, paneId)) return nullptr;
  if (auto found = findResizeSplit(child, paneId, kind)) return found;
  return node->kind == kind ? node : nullptr;
}

bool replacePane(NodePtr& node, int paneId, const NodePtr& replacement) {
  if (!node) return false;
  if (node->type == Node::Type::Leaf) {
    if (!node->pane || node->pane->id() != paneId) return false;
    node = replacement;
    return true;
  }
  if (replacePane(node->first, paneId, replacement)) return true;
  return replacePane(node->second, paneId, replacement);
}

bool replaceNode(NodePtr& node, const NodePtr& target,
                 const NodePtr& replacement) {
  if (!node) return false;
  if (node == target) {
    node = replacement;
    return true;
  }
  if (replaceNode(node->first, target, replacement)) return true;
  return replaceNode(node->second, target, replacement);
}

}  // namespace nexyth
