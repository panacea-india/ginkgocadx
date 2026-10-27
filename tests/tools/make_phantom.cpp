/*
 * This file is part of Ginkgo CADx
 *
 * Ginkgo CADx is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser Public License
 * along with Ginkgo CADx; if not, see <http://www.gnu.org/licenses/>.
 *
 */

// Writes the phantom CT series (see tests/phantom.h) into <dir>/<variant>/.
// Not run by ctest; import the folders into Ginkgo to check the 3D views.
//
// Usage: make_phantom <dir>

#include "../phantom.h"

#include <sys/stat.h>

#include <cerrno>
#include <cstdio>

int main(int argc, char** argv)
{
        if (argc != 2) {
                std::fprintf(stderr, "Usage: %s <dir>\n", argv[0]);
                return 2;
        }
        const std::string root = argv[1];
        if (mkdir(root.c_str(), 0755) != 0 && errno != EEXIST) {
                std::perror(root.c_str());
                return 1;
        }

        for (int i = 0; i < phantom::VariantCount; ++i) {
                const phantom::Variant v = static_cast<phantom::Variant>(i);
                const std::string dir = root + "/" + phantom::Name(v);
                if (mkdir(dir.c_str(), 0755) != 0 && errno != EEXIST) {
                        std::perror(dir.c_str());
                        return 1;
                }
                if (!phantom::Write(v, dir)) {
                        std::fprintf(stderr, "Couldn't write %s\n", dir.c_str());
                        return 1;
                }
                std::printf("%s\n", dir.c_str());
        }
        return 0;
}
