/// Caches (trees, buffers, ...).
///
/// @file

#pragma once
#ifndef CACHE_HPP__NCBG14HO
#define CACHE_HPP__NCBG14HO

#include <map>
#include <mutex>
#include <string>

#include <sys/stat.h>

#include "rewofs/disablewarnings.hpp"
#include <boost/filesystem.hpp>
#include <boost/noncopyable.hpp>
#include <gsl/span>
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

    /// Drop the whole tree.
    void reset();

    Node& get_root();
    Node& make_node(Node& parent, const std::string& name);
    Node& get_node(const Path& name);
    /// Remove a node only if it has no children.
    void remove_single(const Path& path);
    Node& make_node(const Path& path);
    /// Fails if `to` exists.
    void rename(const Path& from, const Path& to);
    void exchange(const Path& node1, const Path& node2);

private:
    Node m_root{};
};

//==========================================================================

/// Files content cache.
/// TODO optimize, very naive now
class Content
{
public:
    /// Delete all content.
    void reset();

    /// @return false if the block was not found
    bool read(const Path& path, const uintmax_t start, const size_t size,
              const std::function<void(const gsl::span<const uint8_t>)>& store_cb);
    void write(const Path& path, const uintmax_t start, std::vector<uint8_t> content);
    /// Delete all blocks related to the path.
    void delete_file(const Path& path);

private:
    struct Block
    {
        Path path{};
        uintmax_t start{};
        std::vector<uint8_t> content{};
    };

    std::deque<Block> m_blocks{};
};

//==========================================================================

/// Wrapper around Tree and Content
class Cache
{
public:
    std::unique_lock<std::mutex> lock();
    void reset();

    Node& get_root();
    Node& make_node(Node& parent, const std::string& name);
    Node& get_node(const Path& name);
    void remove_single(const Path& path);
    Node& make_node(const Path& path);
    void rename(const Path& from, const Path& to);
    void exchange(const Path& node1, const Path& node2);
    bool read(const Path& path, const uintmax_t start, const size_t size,
              const std::function<void(const gsl::span<const uint8_t>)>& store_cb);
    void write(const Path& path, const uintmax_t start, std::vector<uint8_t> content);

private:
    cache::Tree m_tree{};
    cache::Content m_content{};
    std::mutex m_mutex{};
};

//==========================================================================
} // namespace rewofs::client::cache

#endif /* include guard */
