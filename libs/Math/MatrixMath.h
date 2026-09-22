#pragma once

/// @file MatrixMath.h
/// @brief Lightweight header-only fixed-size matrix and vector templates for embedded Kalman filtering.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <stdexcept>

namespace Math {

/// @class Matrix
/// @brief Fixed-size Rows x Cols matrix template with zero dynamic heap allocation.
template <std::size_t Rows, std::size_t Cols>
class Matrix {
public:
    static constexpr std::size_t RowCount = Rows;
    static constexpr std::size_t ColCount = Cols;

    /// @brief Default constructor initializing all elements to zero.
    constexpr Matrix() noexcept
    {
        for (std::size_t r = 0; r < Rows; ++r) {
            for (std::size_t c = 0; c < Cols; ++c) {
                m_data[r][c] = 0.0;
            }
        }
    }

    /// @brief Constructor initializing with nested initializer list.
    Matrix(std::initializer_list<std::initializer_list<double>> list)
    {
        std::size_t r = 0;
        for (const auto& rowList : list) {
            if (r < Rows) {
                std::size_t c = 0;
                for (const auto& val : rowList) {
                    if (c < Cols) {
                        m_data[r][c] = val;
                        ++c;
                    }
                }
                for (; c < Cols; ++c) {
                    m_data[r][c] = 0.0;
                }
                ++r;
            }
        }
        for (; r < Rows; ++r) {
            for (std::size_t c = 0; c < Cols; ++c) {
                m_data[r][c] = 0.0;
            }
        }
    }

    /// @brief Element access operator (row, col).
    [[nodiscard]] constexpr double& operator()(std::size_t r, std::size_t c) noexcept
    {
        return m_data[r][c];
    }

    /// @brief Const element access operator (row, col).
    [[nodiscard]] constexpr const double& operator()(std::size_t r, std::size_t c) const noexcept
    {
        return m_data[r][c];
    }

    /// @brief Generates an identity matrix (for square matrices).
    [[nodiscard]] static constexpr Matrix<Rows, Cols> identity() noexcept
    {
        Matrix<Rows, Cols> res;
        constexpr std::size_t minDim = (Rows < Cols) ? Rows : Cols;
        for (std::size_t i = 0; i < minDim; ++i) {
            res(i, i) = 1.0;
        }
        return res;
    }

    /// @brief Matrix addition.
    [[nodiscard]] Matrix<Rows, Cols> operator+(const Matrix<Rows, Cols>& other) const noexcept
    {
        Matrix<Rows, Cols> res;
        for (std::size_t r = 0; r < Rows; ++r) {
            for (std::size_t c = 0; c < Cols; ++c) {
                res.m_data[r][c] = m_data[r][c] + other.m_data[r][c];
            }
        }
        return res;
    }

    /// @brief Matrix subtraction.
    [[nodiscard]] Matrix<Rows, Cols> operator-(const Matrix<Rows, Cols>& other) const noexcept
    {
        Matrix<Rows, Cols> res;
        for (std::size_t r = 0; r < Rows; ++r) {
            for (std::size_t c = 0; c < Cols; ++c) {
                res.m_data[r][c] = m_data[r][c] - other.m_data[r][c];
            }
        }
        return res;
    }

    /// @brief Scalar multiplication.
    [[nodiscard]] Matrix<Rows, Cols> operator*(double scalar) const noexcept
    {
        Matrix<Rows, Cols> res;
        for (std::size_t r = 0; r < Rows; ++r) {
            for (std::size_t c = 0; c < Cols; ++c) {
                res.m_data[r][c] = m_data[r][c] * scalar;
            }
        }
        return res;
    }

    /// @brief Matrix multiplication: (Rows x K) * (K x OtherCols) -> (Rows x OtherCols).
    template <std::size_t OtherCols>
    [[nodiscard]] Matrix<Rows, OtherCols> operator*(const Matrix<Cols, OtherCols>& other) const noexcept
    {
        Matrix<Rows, OtherCols> res;
        for (std::size_t r = 0; r < Rows; ++r) {
            for (std::size_t oc = 0; oc < OtherCols; ++oc) {
                double sum = 0.0;
                for (std::size_t k = 0; k < Cols; ++k) {
                    sum += m_data[r][k] * other(k, oc);
                }
                res(r, oc) = sum;
            }
        }
        return res;
    }

    /// @brief Computes transpose of the matrix.
    [[nodiscard]] Matrix<Cols, Rows> transpose() const noexcept
    {
        Matrix<Cols, Rows> res;
        for (std::size_t r = 0; r < Rows; ++r) {
            for (std::size_t c = 0; c < Cols; ++c) {
                res(c, r) = m_data[r][c];
            }
        }
        return res;
    }

    /// @brief Computes matrix inverse via Gauss-Jordan elimination with partial pivoting (Square matrices only).
    [[nodiscard]] Matrix<Rows, Cols> inverse() const
    {
        static_assert(Rows == Cols, "Matrix must be square to invert.");
        constexpr std::size_t N = Rows;

        std::array<std::array<double, 2 * N>, N> aug;
        for (std::size_t r = 0; r < N; ++r) {
            for (std::size_t c = 0; c < N; ++c) {
                aug[r][c] = m_data[r][c];
                aug[r][c + N] = (r == c) ? 1.0 : 0.0;
            }
        }

        for (std::size_t i = 0; i < N; ++i) {
            // Pivot selection
            std::size_t maxRow = i;
            double maxVal = std::abs(aug[i][i]);
            for (std::size_t r = i + 1; r < N; ++r) {
                if (std::abs(aug[r][i]) > maxVal) {
                    maxVal = std::abs(aug[r][i]);
                    maxRow = r;
                }
            }

            if (maxVal < 1e-12) {
                // Return identity on singular matrix to prevent hard crashes
                return Matrix<Rows, Cols>::identity();
            }

            if (maxRow != i) {
                std::swap(aug[i], aug[maxRow]);
            }

            const double pivot = aug[i][i];
            for (std::size_t c = 0; c < 2 * N; ++c) {
                aug[i][c] /= pivot;
            }

            for (std::size_t r = 0; r < N; ++r) {
                if (r != i) {
                    const double factor = aug[r][i];
                    for (std::size_t c = 0; c < 2 * N; ++c) {
                        aug[r][c] -= factor * aug[i][c];
                    }
                }
            }
        }

        Matrix<Rows, Cols> inv;
        for (std::size_t r = 0; r < N; ++r) {
            for (std::size_t c = 0; c < N; ++c) {
                inv(r, c) = aug[r][c + N];
            }
        }
        return inv;
    }

    /// @brief Computes lower-triangular Cholesky factor L such that A = L * L^T (Square symmetric matrices only).
    [[nodiscard]] Matrix<Rows, Cols> cholesky() const
    {
        static_assert(Rows == Cols, "Matrix must be square for Cholesky decomposition.");
        constexpr std::size_t N = Rows;
        Matrix<Rows, Cols> L;

        for (std::size_t i = 0; i < N; ++i) {
            for (std::size_t j = 0; j <= i; ++j) {
                double sum = 0.0;
                for (std::size_t k = 0; k < j; ++k) {
                    sum += L(i, k) * L(j, k);
                }

                if (i == j) {
                    const double val = m_data[i][i] - sum;
                    L(i, j) = std::sqrt(std::max(1e-12, val));
                } else {
                    const double diag = L(j, j);
                    L(i, j) = (std::abs(diag) > 1e-12) ? ((m_data[i][j] - sum) / diag) : 0.0;
                }
            }
        }
        return L;
    }

private:
    std::array<std::array<double, Cols>, Rows> m_data {};
};

/// @class Vector
/// @brief Specialization of Matrix as a column vector (Dim x 1).
template <std::size_t Dim>
class Vector : public Matrix<Dim, 1> {
public:
    /// @brief Default constructor zero-initializing all components.
    constexpr Vector() noexcept
        : Matrix<Dim, 1>()
    {
    }

    /// @brief Constructor initializing from initializer list.
    Vector(std::initializer_list<double> list)
    {
        std::size_t i = 0;
        for (const auto& val : list) {
            if (i < Dim) {
                (*this)(i) = val;
                ++i;
            }
        }
    }

    /// @brief Constructor converting from Matrix<Dim, 1>.
    constexpr Vector(const Matrix<Dim, 1>& m) noexcept
        : Matrix<Dim, 1>(m)
    {
    }

    /// @brief Assignment from Matrix<Dim, 1>.
    Vector& operator=(const Matrix<Dim, 1>& m) noexcept
    {
        Matrix<Dim, 1>::operator=(m);
        return *this;
    }

    /// @brief 1D indexing operator.
    [[nodiscard]] constexpr double& operator()(std::size_t i) noexcept
    {
        return Matrix<Dim, 1>::operator()(i, 0);
    }

    /// @brief 1D const indexing operator.
    [[nodiscard]] constexpr const double& operator()(std::size_t i) const noexcept
    {
        return Matrix<Dim, 1>::operator()(i, 0);
    }

    /// @brief 1D bracket indexing.
    [[nodiscard]] constexpr double& operator[](std::size_t i) noexcept
    {
        return Matrix<Dim, 1>::operator()(i, 0);
    }

    /// @brief 1D const bracket indexing.
    [[nodiscard]] constexpr const double& operator[](std::size_t i) const noexcept
    {
        return Matrix<Dim, 1>::operator()(i, 0);
    }

    /// @brief Euclidean L2 norm of the vector.
    [[nodiscard]] double norm() const noexcept
    {
        double sum = 0.0;
        for (std::size_t i = 0; i < Dim; ++i) {
            sum += (*this)[i] * (*this)[i];
        }
        return std::sqrt(sum);
    }

    /// @brief Euclidean dot product with another vector of same dimension.
    [[nodiscard]] double dot(const Vector<Dim>& other) const noexcept
    {
        double sum = 0.0;
        for (std::size_t i = 0; i < Dim; ++i) {
            sum += (*this)[i] * other[i];
        }
        return sum;
    }

    /// @brief Outer product producing a Dim x OtherDim matrix: v * other^T.
    template <std::size_t OtherDim>
    [[nodiscard]] Matrix<Dim, OtherDim> outer(const Vector<OtherDim>& other) const noexcept
    {
        Matrix<Dim, OtherDim> res;
        for (std::size_t r = 0; r < Dim; ++r) {
            for (std::size_t c = 0; c < OtherDim; ++c) {
                res(r, c) = (*this)[r] * other[c];
            }
        }
        return res;
    }
};

/// @brief Commutative scalar multiplication: scalar * Matrix.
template <std::size_t Rows, std::size_t Cols>
[[nodiscard]] inline Matrix<Rows, Cols> operator*(double scalar, const Matrix<Rows, Cols>& mat) noexcept
{
    return mat * scalar;
}

} // namespace Math

namespace PelcoD {
namespace Math = ::Math;
using ::Math::Matrix;
using ::Math::Vector;
} // namespace PelcoD
