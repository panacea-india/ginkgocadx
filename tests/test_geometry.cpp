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

// GNC::GCS::Vector, the 2D vector type the measurement widgets compute with.

#include <gtest/gtest.h>

#include <api/math/geometry.h>

using GNC::GCS::Vector;

TEST(Vector, EuclideanDistance)
{
        EXPECT_NEAR(Vector(0.0, 0.0).DistanciaEuclidea(Vector(3.0, 4.0)), 5.0, 1e-6);
}

TEST(Vector, Midpoint)
{
        const Vector m = Vector(0.0, 0.0).PuntoMedio(Vector(4.0, 2.0));
        EXPECT_NEAR(m.x, 2.0, 1e-6);
        EXPECT_NEAR(m.y, 1.0, 1e-6);
}

TEST(Vector, OrthonormalIsPerpendicularAndUnitLength)
{
        const Vector v(3.0, 4.0);
        const Vector n = v.VectorOrtonormal();
        EXPECT_NEAR(v.x * n.x + v.y * n.y, 0.0, 1e-6);
        EXPECT_NEAR(n.Norma2(), 1.0, 1e-6);
}

TEST(Vector, OrthonormalOfAxisVectors)
{
        const Vector n = Vector(1.0, 0.0).VectorOrtonormal();
        EXPECT_NEAR(n.x, 0.0, 1e-6);
        EXPECT_NEAR(n.y, -1.0, 1e-6);
}

TEST(Vector, NormalizingZeroVectorGivesZero)
{
        Vector v(0.0, 0.0);
        v.Normalizar();
        EXPECT_EQ(v.x, 0.0);
        EXPECT_EQ(v.y, 0.0);
}

TEST(Vector, DistanceToLine)
{
        EXPECT_NEAR(Vector(0.0, 1.0).DistanciaARecta(Vector(0.0, 0.0), Vector(2.0, 0.0)), 1.0, 1e-6);
}
