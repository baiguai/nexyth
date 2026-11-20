#pragma once

#include <memory>

#include "pane.hpp"

namespace nexyth {

class Node;
using NodePtr = std::shared_ptr<Node>;

enum class SplitKind { SideBySide, Stacked };

struct Node {
  enum class Type { Leaf, Split };

  Type type = Type::Leaf;
  SplitKind kind = SplitKind::SideBySide;
  double ratio = 0.5;
  NodePtr first;
  NodePtr second;
  std::shared_ptr<Pane> pane;
};

NodePtr makePaneNode(std::shared_ptr<Pane> pane);
NodePtr makeSplitNode(NodePtr first, NodePtr second, SplitKind kind,
                      double ratio = 0.5);
NodePtr findPane(const NodePtr& node, int paneId);
NodePtr findParent(const NodePtr& node, int paneId);
bool replacePane(NodePtr& node, int paneId, const NodePtr& replacement);

}  // namespace nexyth
