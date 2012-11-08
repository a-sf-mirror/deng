/**
 * @file pathtreenode.cpp
 * @ingroup base
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <de/Log>
#include "pathtree.h"

namespace de {

struct PathTree::Node::Instance
{
    /// @c true = this is a leaf node.
    bool isLeaf;

    /// PathTree which owns this node.
    PathTree& tree;

    /// Unique identifier for the path fragment this node represents,
    /// in the owning PathTree.
    PathTree::FragmentId fragmentId;

    /// Parent node in the user's logical hierarchy.
    PathTree::Node* parent;

    /// User-specified data pointer associated with this node.
    void* userPointer;

    /// User-specified value associated with this node.
    int userValue;

    Instance(PathTree& _tree, bool _isLeaf, PathTree::FragmentId _fragmentId,
             PathTree::Node* _parent)
        : isLeaf(_isLeaf), tree(_tree), fragmentId(_fragmentId), parent(_parent),
          userPointer(0), userValue(0)
    {}
};

PathTree::Node::Node(PathTree& tree, PathTree::NodeType type, PathTree::FragmentId fragmentId,
    PathTree::Node* parent, void* userPointer, int userValue)
{
    d = new Instance(tree, type == PathTree::Leaf, fragmentId, parent);
    setUserPointer(userPointer);
    setUserValue(userValue);
}

PathTree::Node::~Node()
{
    delete d;
}

bool PathTree::Node::isLeaf() const
{
    return d->isLeaf;
}

PathTree& PathTree::Node::tree() const
{
    return d->tree;
}

PathTree::Node* PathTree::Node::parent() const
{
    return d->parent;
}

PathTree::FragmentId PathTree::Node::fragmentId() const
{
    return d->fragmentId;
}

String /*const&*/ PathTree::Node::name() const
{
    return tree().fragmentName(d->fragmentId);
}

ushort PathTree::Node::hash() const
{
    return tree().fragmentHash(d->fragmentId);
}

void* PathTree::Node::userPointer() const
{
    return d->userPointer;
}

int PathTree::Node::userValue() const
{
    return d->userValue;
}

PathTree::Node& PathTree::Node::setUserPointer(void* ptr)
{
    d->userPointer = ptr;
    return *this;
}

PathTree::Node& PathTree::Node::setUserValue(int value)
{
    d->userValue = value;
    return *this;
}

static int matchPathFragment(char const* string, char const* pattern)
{
    char const* in = string, *st = pattern;

    while(*in)
    {
        if(*st == '*')
        {
            st++;
            continue;
        }

        if(*st != '?' && (tolower((unsigned char) *st) != tolower((unsigned char) *in)))
        {
            // A mismatch. Hmm. Go back to a previous '*'.
            while(st >= pattern && *st != '*')
            { st--; }

            // No match?
            if(st < pattern) return false;

            // The asterisk lets us continue.
        }

        // This character of the pattern is OK.
        st++;
        in++;
    }

    // Skip remaining asterisks.
    while(*st == '*')
    { st++; }

    // Match is good if the end of the pattern was reached.
    return *st == 0;
}

int PathTree::Node::comparePath(PathMap& searchPattern, int flags) const
{
    if(((flags & PCF_NO_LEAF)   && isLeaf()) ||
       ((flags & PCF_NO_BRANCH) && !isLeaf()))
        return 1;

    PathMapFragment const* sfragment = PathMap_Fragment(&searchPattern, 0);
    if(!sfragment) return 1; // Hmm...

    // In reverse order, compare path fragments in the search term.
    uint fragmentCount = PathMap_Size(&searchPattern);

    PathTree::Node const* node = this;
    for(uint i = 0; i < fragmentCount; ++i)
    {
        if(!sfragment->isWild())
        {
            // If the hashes don't match it can't possibly be this.
            if(sfragment->hash != node->hash())
            {
                return 1;
            }

            // Compare the path fragment to that of the search term.
            /// @todo Optimize: copying sfragment is unnecessary.
            char buf[256];
            int sfragmentLength = sfragment->length();
            qstrncpy(buf, sfragment->from, sfragmentLength + 1);

            String /*const&*/ fragment = node->name();
            QByteArray fragmentUtf8 = fragment.toUtf8();
            if(!matchPathFragment(fragmentUtf8.constData(), buf))
            {
                return 1;
            }
        }

        // Have we arrived at the search target?
        if(i == fragmentCount - 1)
        {
            return !(!(flags & PCF_MATCH_FULL) || !node->parent());
        }

        // Is the hierarchy too shallow?
        if(!node->parent())
        {
            return 1;
        }

        // So far so good. Move one level up the hierarchy.
        node = node->parent();
        sfragment = PathMap_Fragment(&searchPattern, i + 1);
    }

    return 1;
}

#ifdef LIBDENG_STACK_MONITOR
static void* stackStart;
static size_t maxStackDepth;
#endif

typedef struct pathconstructorparams_s {
    size_t length;
    String& composedPath;
    char delimiter;
} pathconstructorparams_t;

/**
 * Recursive path constructor. First finds the root and the full length of the
 * path (when descending), then allocates memory for the string, and finally
 * copies each fragment with the delimiters (on the way out).
 */
static void pathConstructor(pathconstructorparams_t& parm, PathTree::Node const& trav)
{
    String /*const&*/ fragment = trav.name();

#ifdef LIBDENG_STACK_MONITOR
    maxStackDepth = MAX_OF(maxStackDepth, stackStart - (void*)&fragment);
#endif

    parm.length += fragment.length();

    if(trav.parent())
    {
        if(parm.delimiter)
        {
            // There also needs to be a delimiter (a single character).
            parm.length += 1;
        }

        // Descend to parent level.
        pathConstructor(parm, *trav.parent());

        // Append the separator.
        if(parm.delimiter)
            parm.composedPath.append(parm.delimiter);
    }
    // We've arrived at the deepest level. The full length is now known.
    // Ensure there's enough memory for the string.
    else if(parm.composedPath)
    {
        parm.composedPath.reserve(parm.length);
    }

    // Assemble the path by appending the fragment.
    parm.composedPath.append(fragment);
}

/**
 * @todo This is a good candidate for result caching: the constructed path
 * could be saved and returned on subsequent calls. Are there any circumstances
 * in which the cached result becomes obsolete? -jk
 *
 * The only times the result becomes obsolete is when the delimiter is changed
 * or when the directory itself is rebuilt (in which case the nodes themselves
 * will be free'd). Note that any caching mechanism should not counteract one
 * of the primary goals of this class, i.e., optimal memory usage for the whole
 * directory. Caching constructed paths for every leaf node in the directory
 * would completely negate the benefits of the design of this class.
 *
 * Perhaps a fixed size MRU cache? -ds
 */
String PathTree::Node::composePath(char delimiter) const
{
    String result;

    pathconstructorparams_t parm = { 0, result, delimiter };
#ifdef LIBDENG_STACK_MONITOR
    stackStart = &parm;
#endif

    // Include a terminating path delimiter for branches.
    if(delimiter && !isLeaf())
        parm.length += 1; // A single character.

    // Recursively construct the path from fragments and delimiters.
    pathConstructor(parm, *this);

    // Terminating delimiter for branches.
    if(delimiter && !isLeaf())
        result.append(delimiter);

    DENG2_ASSERT(result.size() == parm.length);

#ifdef LIBDENG_STACK_MONITOR
    LOG_AS("pathConstructor");
    LOG_INFO("Max stack depth: %1 bytes") << maxStackDepth;
#endif

    return result;
}

} // namespace de
