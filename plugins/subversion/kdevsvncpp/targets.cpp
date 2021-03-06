/*
 * ====================================================================
 * Copyright (c) 2002-2009 The RapidSvn Group.  All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the file GPL.txt.  
 * If not, see <http://www.gnu.org/licenses/>.
 *
 * This software consists of voluntary contributions made by many
 * individuals.  For exact contribution history, see the revision
 * history and logs, available at http://rapidsvn.tigris.org/.
 * ====================================================================
 */

// subversion api
#include "svn_types.h"

// apr api
#include "apr_pools.h"
#include "apr_strings.h"

// svncpp
#include "kdevsvncpp/targets.hpp"
#include "kdevsvncpp/path.hpp"
#include "kdevsvncpp/pool.hpp"

namespace svn
{
  Targets::Targets(const PathVector & targets)
  {
    m_targets = targets;
  }

  Targets::Targets(const apr_array_header_t * apr_targets)
  {
    int i;

    m_targets.clear();
    m_targets.reserve(apr_targets->nelts);

    for (i = 0; i < apr_targets->nelts; i++)
    {
      const char ** target =
        &APR_ARRAY_IDX(apr_targets, i, const char *);

      m_targets.push_back(Path(*target));
    }
  }

  Targets::Targets(const Targets & targets)
  {
    m_targets = targets.targets();
  }

  Targets::Targets(const char * target)
  {
    if (target != 0)
    {
      m_targets.push_back(target);
    }
  }

  Targets::~Targets()
  {
  }

  const apr_array_header_t *
  Targets::array(const Pool & pool) const
  {
    PathVector::const_iterator it;

    apr_pool_t *apr_pool = pool.pool();
    apr_array_header_t *apr_targets =
      apr_array_make(apr_pool,
                     m_targets.size(),
                     sizeof(const char *));

    for (it = m_targets.begin(); it != m_targets.end(); it++)
    {
      const Path &path = *it;
      const char * target =
        apr_pstrdup(apr_pool, path.c_str());

      (*((const char **) apr_array_push(apr_targets))) = target;
    }

    return apr_targets;
  }

  const PathVector &
  Targets::targets() const
  {
    return m_targets;
  }

  size_t
  Targets::size() const
  {
    return m_targets.size();
  }

  const Path
  Targets::target() const
  {
    if (m_targets.size() > 0)
    {
      return m_targets[0];
    }
    else
    {
      return "";
    }
  }

  void
  Targets::push_back(const Path & path)
  {
    m_targets.push_back(path);
  }

  void
  Targets::clear()
  {
    m_targets.clear();
  }

  void
  Targets::reserve(size_t size)
  {
    m_targets.reserve(size);
  }
}

/* -----------------------------------------------------------------
 * local variables:
 * eval: (load-file "../../rapidsvn-dev.el")
 * end:
 */
