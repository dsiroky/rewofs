/// Caches (trees, buffers, ...).
///
/// @file

#pragma once
#ifndef CACHE_HPP__NCBG14HO
#define CACHE_HPP__NCBG14HO

#include <string>
#include <map>

#include <sys/stat.h>

#include "rewofs/disablewarnings.hpp"
#include <boost/filesystem.hpp>
#include <boost/noncopyable.hpp>
#include "rewofs/enablewarnings.hpp"

//==========================================================================
namespace rewofs::client::cache {
//==========================================================================

using Path = boost::filesystem::path;

//==========================================================================

struct Node
{
    std::string name{};
    struct stat st{};
    // TODO better map structure
    std::map<std::string, Node> children{};

    Node(const Node&) = delete;
    Node(Node&&) = default;
};

//==========================================================================

class Tree
{
public:
    Tree();

    Node& raw_get_root();
    Node& raw_make_node(Node& parent, const std::string& name);
    struct stat lstat(const Path& path);
    void chmod(const Path& path, const mode_t mode);
    /// Remove a node only if it has no children.
    void remove_single(const Path& path);
    Node& make_node(const Path& path);
    void rename(const Path& from, const Path& to);

private:
    Node& get_node(const Path& name);

    Node m_root{};
};

//==========================================================================
} // namespace rewofs::client::cache

#endif /* include guard */
