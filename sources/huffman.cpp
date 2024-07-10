#include <huffman.h>
#include <optional>
#include <iostream>
#include <numeric>

struct Node {
    std::optional<int> value;  // mb int -> cock
    size_t height;
    std::unique_ptr<Node> left{}, right{};
    Node *parent;  // should not kill him (that would be sad)

    Node(std::optional<int> value, size_t height, Node *parent)
        : value(value), height(height), parent(parent) {
    }
};

class HuffmanTree::Impl {
public:
    Impl() : root_(std::make_unique<Node>(std::nullopt, 0, nullptr)), cur_(root_.get()) {
    }

    bool Move(bool bit, int &value) {
        if (!cur_) {
            throw std::invalid_argument("attempt to move from nowhere");
        }
        Node *need;
        if (bit) {
            need = cur_->right.get();
        } else {
            need = cur_->left.get();
        }
        if (need) {
            if (!need->value.has_value()) {
                cur_ = need;
                return false;
            }
            value = need->value.value();
            cur_ = root_.get();
            return true;
        } else {
            throw std::invalid_argument("attempt to move to nowhere");
        }
    }

    bool AddVertex(uint8_t code_len, uint8_t value) {
        if (cur_->value.has_value()) {
            cur_ = cur_->parent;
            return false;
        }
        if (code_len == cur_->height) {
            // found
            cur_->value = value;
            cur_ = cur_->parent;
            return true;
        }
        if (!cur_->left) {
            // no left son
            cur_->left = std::make_unique<Node>(std::nullopt, cur_->height + 1, cur_);
        }
        {
            // go left
            cur_ = cur_->left.get();
            bool res = AddVertex(code_len, value);
            if (res) {
                cur_ = cur_->parent;
                return true;
            }
        }
        if (!cur_->right) {
            // no right son
            cur_->right = std::make_unique<Node>(std::nullopt, cur_->height + 1, cur_);
        }
        {
            // go right
            cur_ = cur_->right.get();
            bool res = AddVertex(code_len, value);
            if (res) {
                cur_ = cur_->parent;
                return true;
            }
        }
        cur_ = cur_->parent;
        return false;
    }

    void SetCurNode(Node *node) {
        cur_ = node;
    }

    Node *GetRoot() const {
        return root_.get();
    }

private:
    std::unique_ptr<Node> root_;
    Node *cur_;
};

HuffmanTree::HuffmanTree() {
    impl_ = std::make_unique<Impl>();
}

void HuffmanTree::Build(const std::vector<uint8_t> &code_lengths,
                        const std::vector<uint8_t> &values) {
    if (std::accumulate(code_lengths.begin(), code_lengths.end(), static_cast<size_t>(0)) !=
        values.size()) {
        throw std::invalid_argument("sum(code_lengths) != values.size()");
    }
    impl_ = std::make_unique<Impl>();
    if (code_lengths.size() > 16) {  // magic number.
        throw std::invalid_argument("too big array in build");
    }

    size_t code_ind = 0;
    uint8_t used = 0;
    for (uint8_t i : values) {
        if (code_lengths[code_ind] == used) {
            used = 0;
            code_ind++;
            while (code_ind < code_lengths.size() && code_lengths[code_ind] == used) {
                code_ind++;
            }
        }
        if (code_ind == code_lengths.size()) {
            throw std::invalid_argument("wrong code_lengths len in huffman build");
        }
        try {
            bool added = impl_->AddVertex(code_ind + 1, i);  // zero-based bruh
            if (!added) {
                throw std::invalid_argument("can't add one more code to huffman");
            }
            used++;
        } catch (...) {
            // well that's bruh
            throw std::invalid_argument("can't add one more code to huffman");
        }
        impl_->SetCurNode(impl_->GetRoot());
    }
    impl_->SetCurNode(impl_->GetRoot()); // wtf why
}

bool HuffmanTree::Move(bool bit, int &value) {
    return impl_->Move(bit, value);
}

HuffmanTree::HuffmanTree(HuffmanTree &&) = default;

HuffmanTree &HuffmanTree::operator=(HuffmanTree &&) = default;

HuffmanTree::~HuffmanTree() = default;
