#pragma once
#include "basic/allocator.h"
#include "struct/copy.h"
#include "struct/print.h"

struct Data_Tree_Node {
    std::vector<size_t> children;
};

using Hash    = std::array<size_t, 16>;
using Node_Id = size_t;

// struct Root {
//     Node_Id node_id = SIZE_MAX;
// };
struct Root_Id {
    size_t id = SIZE_MAX;
};

struct Data_Tree {
    std::vector<Data_Tree_Node> nodes;
    std::vector<size_t>         offsets;

    // root -> (node -> parent)
    std::vector<std::unordered_map<Node_Id, Node_Id>> parents;
    std::unordered_map<size_t, Node_Id>               node_from_value;
    Allocator_Linear                                  allocator;
    Copier                                            copier;

    std::vector<Node_Id> roots;  // List of roots of trees.
    // std::vector<Node_Id> history;   // Ordered list of roots.
    size_t active_root_index = -1;  // Index in history of current tree.

    Data_Tree(size_t capacity) : allocator(capacity), copier(&allocator) {}

    // Root active_root() const { return roots[active_root_index]; }

    void set_parent(Root_Id root, Node_Id node, Node_Id parent) {
        parents[root.id][node] = parent;
    }

    Node_Id get_parent(Root_Id root, Node_Id node) const {
        return parents[root.id].at(node);
    }

    template <typename T>
    Node_Id get_node(const T& value) const {
        return node_from_value.at(offset(value));
    }

    template <typename T>
    size_t offset(const T& value) const {
        return (size_t)((byte*)&value - allocator.data());
    }

    template <typename T>
    Node_Id node_id(const T& value) const {
        auto o = offset(value);
        return node_from_value.at(o);
    }

    template <typename T>
    const T& value_from_node(Node_Id node_index) const {
        return (T*)(allocator.data() + offsets[node_index]);
    }

    template <typename T>
    T& copy(const T& value) {
        this->copier.copy(value);
        return allocator.allocate<T>();
    }
};

template <typename T>
static Node_Id make_tree(Data_Tree& tree, Root_Id root, const T& value) {
    // value is assumed to be already in the tree.allocator
    auto  node_id = tree.nodes.size();
    auto& node    = tree.nodes.emplace_back();

    auto o = tree.offset(value);
    tree.offsets.push_back(o);
    tree.node_from_value[o] = node_id;

    visit_struct::for_each(value, [&](const char* name, auto& value) {
        auto child_id = make_tree(tree, value);
        tree.set_parent(root, child_id, node_id);
    });
    return node_id;
}
static Root_Id make_root(Data_Tree& tree, Root_Id old_root) {
    auto root_id = Root_Id{tree.roots.size()};
    auto node_id = tree.nodes.size();
    tree.roots.push_back(-1);
    tree.parents.push_back({});
    // tree.history.push_back(root_id);
    return root_id;
}

// template <typename T>
// inline Data_Tree make_tree(const T& t, size_t initial_capacity = 1e6) {
//     auto tree = Data_Tree(new Memory(initial_capacity));
//     make_tree(tree, t, 0);
//     return tree;
// }

template <typename T>
static T& editable(Data_Tree& tree, Root_Id root, const T& value) {
    auto& new_value = tree.copy(value);
    return new_value;
}

template <typename T>
static Root_Id commit(Data_Tree& tree, Root_Id old_root, const T& new_value) {
    // Create a whole new tree. For now it's empty, it has a null root. On
    // commit it will bet filled, but it will share most of the nodes with the
    // old tree.
    auto new_root = make_root(tree, old_root);

    // Add new subtree which is just a copy for now. The parent is the same, but
    // we are gonna change it just after. Importanly, this subtree has different
    // memory offsets as the data was copied.
    auto node_id = make_tree(tree, new_root, new_value);

    auto old_node_id   = tree.node_id(new_value);
    auto old_parent_id = tree.get_parent(old_root, old_node_id);
    while (true) {
        if (old_parent_id == SIZE_MAX) break;

        auto  new_parent_id = tree.nodes.size();
        auto& new_parent    = tree.nodes.push_back(
            tree.nodes[old_parent_id]);  // copy

        auto index = SIZE_MAX;
        for (size_t i = 0; i < new_parent.children.size(); i++) {
            if (new_parent.children[i] == node_id) {
                index = i;
                break;
            }
        }
        new_parent.children[index] = node_id;
        tree.set_parent(new_root, node_id, new_parent_id);

        // Move up on:
        node_id       = new_parent_id;                             // new tree
        old_parent_id = tree.get_parent(old_root, old_parent_id);  // old tree
    }

    // Set root of the new tree
    tree.roots[new_root.id] = node_id;
    return new_root;
}

template <typename T>
static bool are_equal(const Data_Tree& tree, const T& value0, const T& value1) {
    auto node0 = tree.node_id(value0);
    auto node1 = tree.node_id(value1);
    if (node0 == node1) {
        return true;
    }

    if (tree.nodes[node0].children.size() !=
        tree.nodes[node1].children.size()) {
        return false;
    }
    for (size_t i = 0; i < tree.nodes[node0].children.size(); i++) {
        auto equal = are_equal(tree, tree.nodes[node0].children[i],
                               tree.nodes[node1].children[i]);
        if (!equal) return false;
    }
    return true;
}