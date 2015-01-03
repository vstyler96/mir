/*
 * Copyright © 2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#include "mir/compositor/gl_program_family.h"

namespace mir { namespace compositor {

GLProgramFamily::Shader::~Shader()
{
    if (id)
        glDeleteShader(id);
}

void GLProgramFamily::Shader::init(GLenum type, const char* src)
{
    if (!id)
    {
        id = glCreateShader(type);
        if (id)
        {
            glShaderSource(id, 1, &src, NULL);
            glCompileShader(id);
            GLint ok;
            glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
            if (!ok)
            {
                GLchar log[1024];
                glGetShaderInfoLog(id, sizeof log - 1, NULL, log);
                log[sizeof log - 1] = '\0';
                glDeleteShader(id);
                id = 0;
                throw std::runtime_error(std::string("Compile failed: ")+
                                         log + " for:\n" + src);
            }
        }
    }
}

GLProgramFamily::Program::~Program()
{
    if (id)
        glDeleteProgram(id);
}

GLuint GLProgramFamily::add_program(const char* vshader_src,
                                    const char* fshader_src)
{
    auto& v = vshader[vshader_src];
    if (!v.id) v.init(GL_VERTEX_SHADER, vshader_src);

    auto& f = fshader[fshader_src];
    if (!f.id) f.init(GL_FRAGMENT_SHADER, fshader_src);

    auto& p = program[{v.id, f.id}];
    if (!p.id)
    {
        p.id = glCreateProgram();
        program_ids.insert(p.id);
        glAttachShader(p.id, v.id);
        glAttachShader(p.id, f.id);
        glLinkProgram(p.id);
        GLint ok;
        glGetProgramiv(p.id, GL_LINK_STATUS, &ok);
        if (!ok)
        {
            GLchar log[1024];
            glGetProgramInfoLog(p.id, sizeof log - 1, NULL, log);
            log[sizeof log - 1] = '\0';
            glDeleteShader(p.id);
            p.id = 0;
            throw std::runtime_error(std::string("Link failed: ")+log);
        }
    }

    return p.id;
}

GLint GLProgramFamily::get_uniform_location(GLuint program_id,
                                            const char* name) const
{
    auto& u = uniform[{program_id, name}];
    if (u.id < 0)
        u.id = glGetUniformLocation(program_id, name);
    return u.id;
}

GLint GLProgramFamily::get_attrib_location(GLuint program_id,
                                           const char* name) const
{
    auto& a = attrib[{program_id, name}];
    if (a.id < 0)
        a.id = glGetAttribLocation(program_id, name);
    return a.id;
}

const std::unordered_set<GLuint>& GLProgramFamily::all() const
{
    return program_ids;
}

}} // namespace mir::compositor
