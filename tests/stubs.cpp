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

// Symbols CADxCore expects the application to provide. The controllers that
// reference them are linked into the tests but their extension loading is
// never run, so these only satisfy the linker.

class PrivateExtension;

PrivateExtension* init_VisualizatorExtension()
{
        return 0;
}

// wxWidgets needs an application object before its string and file helpers
// work; a console one is enough, no display is involved.

#include <gtest/gtest.h>
#include <wx/init.h>

namespace
{

class WxEnvironment : public ::testing::Environment
{
public:
        virtual void SetUp()
        {
                wxInitialize();
        }

        virtual void TearDown()
        {
                wxUninitialize();
        }
};

const ::testing::Environment* const wxEnvironment = ::testing::AddGlobalTestEnvironment(new WxEnvironment);

}
