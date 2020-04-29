#include <iostream>
/// @copydoc cache.hpp
///
/// @file

#include "rewofs/client/cache.hpp"

//==========================================================================
namespace rewofs::client::cache {
//==========================================================================

static bool path_has_prefix(const Path& path, const Path& prefix)
{
    auto pair = std::mismatch(path.begin(), path.end(), prefix.begin(), prefix.end());
    return pair.second == prefix.end();
}

//==========================================================================

Tree::Tree()
{
    m_root.name = ".";
    // directory with read permissions
    m_root.st.st_mode = 040444;
}

//--------------------------------------------------------------------------

Node& Tree::get_root()
{
    return m_root;
}

//--------------------------------------------------------------------------

Node& Tree::make_node(Node& parent, const std::string& name)
{
    assert(parent.children.find(name) == parent.children.end());
    Node new_node{};
    new_node.name = name;
    auto [it, inserted] = parent.children.emplace(name, std::move(new_node));
    assert(inserted); (void) inserted;
    return it->second;
}

//--------------------------------------------------------------------------

void Tree::reset()
{
    m_root.children.clear();
}

//--------------------------------------------------------------------------

void Tree::remove_single(const Path& path)
{
    if (path == "/")
    {
        throw std::system_error{EACCES, std::generic_category()};
    }
    auto& parent_node = get_node(path.parent_path());
    const auto nit = parent_node.children.find(path.filename().native());
    if (nit == parent_node.children.end())
    {
        throw std::system_error{ENOENT, std::generic_category()};
    }
    if (nit->second.children.size() > 0)
    {
        throw std::system_error{ENOTEMPTY, std::generic_category()};
    }
    parent_node.children.erase(nit);
}

//--------------------------------------------------------------------------

Node& Tree::make_node(const Path& path)
{
    if (path == "/")
    {
        throw std::system_error{EEXIST, std::generic_category()};
    }
    auto& parent_node = get_node(path.parent_path());
    const auto nit = parent_node.children.find(path.filename().native());
    if (nit != parent_node.children.end())
    {
        throw std::system_error{EEXIST, std::generic_category()};
    }
    Node new_node{};
    new_node.name = path.filename().string();
    auto [it, inserted]
        = parent_node.children.emplace(path.filename().string(), std::move(new_node));
    assert(inserted); (void) inserted;
    return it->second;
}

//--------------------------------------------------------------------------

void Tree::rename(const Path& from, const Path& to)
{
    if ((from == "/") or (to == "/"))
    {
        throw std::system_error{EEXIST, std::generic_category()};
    }

    auto& parent_from = get_node(from.parent_path());
    const auto nit_from = parent_from.children.find(from.filename().native());
    if (nit_from == parent_from.children.end())
    {
        throw std::system_error{ENOENT, std::generic_category()};
    }

    auto& parent_to = get_node(to.parent_path());
    const auto nit_to = parent_to.children.find(to.filename().native());
    if (nit_to != parent_to.children.end())
    {
        throw std::system_error{EEXIST, std::generic_category()};
    }

    Node tmp{std::move(nit_from->second)};
    parent_from.children.erase(nit_from);
    auto [it, inserted]
        = parent_to.children.emplace(to.filename().string(), std::move(tmp));
    (void) it;
    assert(inserted); (void) inserted;
}

//--------------------------------------------------------------------------

void Tree::exchange(const Path& path1, const Path& path2)
{
    if (path_has_prefix(path1, path2) or path_has_prefix(path2, path1))
    {
        throw std::system_error{EINVAL, std::generic_category()};
    }

    auto& node1 = get_node(path1);
    auto& node2 = get_node(path2);

    std::swap(node1.st, node2.st);
    std::swap(node1.children, node2.children);
}

//--------------------------------------------------------------------------

Node& Tree::get_node(const Path& path)
{
    assert((path.native().size() > 0) and (path.native()[0] == '/'));

    Node* node = &m_root;
    auto it = path.begin();
    // skip the initial "/"
    ++it;
    for (; it != path.end(); ++it)
    {
        const auto nit = node->children.find(it->native());
        if (nit == node->children.end())
        {
            throw std::system_error{ENOENT, std::generic_category()};
        }
        node = &nit->second;
    }

    assert(node != nullptr);
    return *node;
}

//==========================================================================

void Content::reset()
{
    m_blocks.clear();
}

//--------------------------------------------------------------------------

bool Content::read(const Path& path, const uintmax_t start, const size_t size,
                   const std::function<void(const gsl::span<const uint8_t>)> store_cb)
{
    const auto it
        = std::find_if(m_blocks.begin(), m_blocks.end(), [&](const auto& block) {
              return (block.path == path) and (block.start == start)
                     and (block.content.size() == size);
          });
    if (it == m_blocks.end())
    {
        return false;
    }
    store_cb(it->content);
    return true;
}

//--------------------------------------------------------------------------

void Content::write(const Path& path, const uintmax_t start, std::vector<uint8_t> content)
{
    const auto it
        = std::find_if(m_blocks.begin(), m_blocks.end(), [&](const auto& block) {
              return (block.path == path) and (block.start <= start)
                     and (block.content.size() >= (start - block.start + content.size()));
          });
    if (it == m_blocks.end())
    {
        Block b{};
        b.path = path;
        b.start = start;
        b.content = content;
        m_blocks.emplace_back(std::move(b));
    }
    else
    {
        std::copy(
            content.begin(), content.end(),
            std::next(it->content.begin(), static_cast<ssize_t>(start - it->start)));
    }
}

//--------------------------------------------------------------------------

void Content::delete_file(const Path& path)
{
    m_blocks.erase(
        std::remove_if(m_blocks.begin(), m_blocks.end(),
                       [&path](const auto& block) { return block.path == path; }),
        m_blocks.end());
}

//==========================================================================
} // namespace rewofs::client::cache
