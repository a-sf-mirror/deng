/**
 * @file quadtreevisual.cpp
 * Graphical Quadtree visual. @ingroup render
 *
 * @author Copyright &copy; 2009-2012 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_render.h"
#include "m_vector.h"

#include "quadtreevisual.h"

#define UNIT_WIDTH                     1
#define UNIT_HEIGHT                    1

static int drawCell(QuadtreeBase::TreeBase* node, void* /*parameters*/)
{
    vec2f_t topLeft, bottomRight;

    V2f_Set(topLeft, UNIT_WIDTH * node->x(), UNIT_HEIGHT * node->y());
    V2f_Set(bottomRight, UNIT_WIDTH  * (node->x() + node->size()),
                         UNIT_HEIGHT * (node->y() + node->size()));

    glBegin(GL_LINE_LOOP);
        glVertex2fv((GLfloat*)topLeft);
        glVertex2f(bottomRight[0], topLeft[1]);
        glVertex2fv((GLfloat*)bottomRight);
        glVertex2f(topLeft[0], bottomRight[1]);
    glEnd();
    return 0; // Continue iteration.
}

void Rend_QuadtreeDebug(QuadtreeBase& quadtree)
{
    // We'll be changing the color, so query the current and restore later.
    GLfloat oldColor[4];
    glGetFloatv(GL_CURRENT_COLOR, oldColor);

    /**
     * Draw our Quadtree.
     */
    QuadtreeBase::TreeBase& root = quadtree;
    glColor4f(1.f, 1.f, 1.f, 1.f / root.size());

    QuadtreeBase::traverse_parameters_t travParms;
    travParms.leafOnly = false;
    travParms.callback = drawCell;
    travParms.callbackParameters = 0;
    quadtree.traverse(&root, travParms);

    /**
     * Draw our bounds.
     */
    vec2f_t start, end;
    V2f_Set(start, 0, 0);
    V2f_Set(end, UNIT_WIDTH * quadtree.width(), UNIT_HEIGHT * quadtree.height());

    glColor3f(1, .5f, .5f);
    glBegin(GL_LINES);
        glVertex2f(start[0], start[1]);
        glVertex2f(  end[0], start[1]);

        glVertex2f(  end[0], start[1]);
        glVertex2f(  end[0],   end[1]);

        glVertex2f(  end[0],   end[1]);
        glVertex2f(start[0],   end[1]);

        glVertex2f(start[0],   end[1]);
        glVertex2f(start[0], start[1]);
    glEnd();

    // Restore GL state.
    glColor4fv(oldColor);
}

#undef UNIT_HEIGHT
#undef UNIT_WIDTH
