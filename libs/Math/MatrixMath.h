#pragma once

/// @file MatrixMath.h
/// @brief Lightweight header-only fixed-size matrix and vector templates for embedded Kalman filtering.

#include <cmath>
#include <cstddef>

#include <algorithm>
#include <array>
#include <initializer_list>
#include <optional>
#include <utility>

namespace Math {

/// @class Matrix
/// @brief Fixed-size Rows x Cols matrix template with zero dynamic heap allocation.
/// @details Provides deterministic stack-allocated matrix arithmetic, transposition, Gauss-Jordan inversion
///          with partial pivoting, and Cholesky decomposition for embedded state estimation.
template <std::size_t Rows, std::size_t Cols> class Matrix {
    static_assert(Rows > 0U && Cols > 0U, "Matrix dimensions must be strictly positive.");

public:
    static constexpr std::size_t RowCount = Rows;
    static constexpr std::size_t ColCount = Cols;

    /// @brief Default constructor initializing all elements to zero.
    /// @details Zero-initializes the underlying fixed-size 2D std::array storage.
    constexpr Matrix() noexcept
    {
        for (std::size_t r = 0U; r < Rows; ++r) {
            for (std::size_t c = 0U; c < Cols; ++c) {
                m_data[r][c] = 0.0;
            }
        }
    }

    /// @brief Constructor initializing with nested initializer list.
    /// @details Populates up to Rows x Cols elements and zero-pads any omitted entries.
    /// @param[in] list Nested initializer list of row values.
    Matrix(std::initializer_list<std::initializer_list<double>> list) noexcept
    {
        std::size_t r = 0U;
        for (const auto& rowList : list) {
            if (r < Rows) {
                std::size_t c = 0U;
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
            for (std::size_t c = 0U; c < Cols; ++c) {
                m_data[r][c] = 0.0;
            }
        }
    }

    /// @brief Mutable element access operator (row, col).
    /// @details Accesses matrix element at (r, c). Indices are clamped to valid bounds for safety.
    /// @param[in] r Row index [0, Rows - 1].
    /// @param[in] c Column index [0, Cols - 1].
    /// @return Reference to element at (r, c).
    [[nodiscard]] constexpr double& operator()(std::size_t r, std::size_t c) noexcept
    {
        const std::size_t safeR = (r < Rows) ? r : (Rows - 1U);
        const std::size_t safeC = (c < Cols) ? c : (Cols - 1U);
        return m_data[safeR][safeC];
    }

    /// @brief Const element access operator (row, col).
    /// @details Accesses matrix element at (r, c). Indices are clamped to valid bounds for safety.
    /// @param[in] r Row index [0, Rows - 1].
    /// @param[in] c Column index [0, Cols - 1].
    /// @return Const reference to element at (r, c).
    [[nodiscard]] constexpr const double& operator()(std::size_t r, std::size_t c) const noexcept
    {
        const std::size_t safeR = (r < Rows) ? r : (Rows - 1U);
        const std::size_t safeC = (c < Cols) ? c : (Cols - 1U);
        return m_data[safeR][safeC];
    }

    /// @brief Generates an identity matrix (ones on principal diagonal, zeros elsewhere).
    /// @details Places 1.0 along the main diagonal up to min(Rows, Cols).
    /// @return Identity matrix of dimension Rows x Cols.
    [[nodiscard]] static constexpr Matrix<Rows, Cols> identity() noexcept
    {
        Matrix<Rows, Cols> res {};
        constexpr std::size_t minDim = (Rows < Cols) ? Rows : Cols;
        for (std::size_t i = 0U; i < minDim; ++i) {
            res(i, i) = 1.0;
        }
        return res;
    }

    /// @brief Element-wise matrix addition.
    /// @details Computes C = A + B.
    /// @param[in] other Right-hand side matrix of identical dimensions.
    /// @return Sum matrix.
    [[nodiscard]] Matrix<Rows, Cols> operator+(const Matrix<Rows, Cols>& other) const noexcept
    {
        Matrix<Rows, Cols> res {};
        for (std::size_t r = 0U; r < Rows; ++r) {
            for (std::size_t c = 0U; c < Cols; ++c) {
                res.m_data[r][c] = m_data[r][c] + other.m_data[r][c];
            }
        }
        return res;
    }

    /// @brief Element-wise matrix subtraction.
    /// @details Computes C = A - B.
    /// @param[in] other Right-hand side matrix of identical dimensions.
    /// @return Difference matrix.
    [[nodiscard]] Matrix<Rows, Cols> operator-(const Matrix<Rows, Cols>& other) const noexcept
    {
        Matrix<Rows, Cols> res {};
        for (std::size_t r = 0U; r < Rows; ++r) {
            for (std::size_t c = 0U; c < Cols; ++c) {
                res.m_data[r][c] = m_data[r][c] - other.m_data[r][c];
            }
        }
        return res;
    }

    /// @brief Scalar multiplication.
    /// @details Scales every element of the matrix by scalar.
    /// @param[in] scalar Multiplier value.
    /// @return Scaled matrix.
    [[nodiscard]] Matrix<Rows, Cols> operator*(double scalar) const noexcept
    {
        Matrix<Rows, Cols> res {};
        for (std::size_t r = 0U; r < Rows; ++r) {
            for (std::size_t c = 0U; c < Cols; ++c) {
                res.m_data[r][c] = m_data[r][c] * scalar;
            }
        }
        return res;
    }

    /// @brief Matrix multiplication: (Rows x Cols) * (Cols x OtherCols) -> (Rows x OtherCols).
    /// @details Computes standard inner product across shared dimension Cols.
    /// @param[in] other Right-hand matrix with Cols rows and OtherCols columns.
    /// @return Product matrix of dimension Rows x OtherCols.
    template <std::size_t OtherCols>
    [[nodiscard]] Matrix<Rows, OtherCols> operator*(const Matrix<Cols, OtherCols>& other) const noexcept
    {
        Matrix<Rows, OtherCols> res {};
        for (std::size_t r = 0U; r < Rows; ++r) {
            for (std::size_t oc = 0U; oc < OtherCols; ++oc) {
                double sum = 0.0;
                for (std::size_t k = 0U; k < Cols; ++k) {
                    sum += m_data[r][k] * other(k, oc);
                }
                res(r, oc) = sum;
            }
        }
        return res;
    }

    /// @brief Computes transpose of the matrix.
    /// @details Swaps rows and columns to produce a Cols x Rows matrix.
    /// @return Transposed matrix of dimension Cols x Rows.
    [[nodiscard]] Matrix<Cols, Rows> transpose() const noexcept
    {
        Matrix<Cols, Rows> res {};
        for (std::size_t r = 0U; r < Rows; ++r) {
            for (std::size_t c = 0U; c < Cols; ++c) {
                res(c, r) = m_data[r][c];
            }
        }
        return res;
    }

    /// @brief Attempts to compute matrix inverse via Gauss-Jordan elimination with partial pivoting.
    /// @details Returns std::nullopt if the matrix is singular or near-singular (pivot < 1e-12).
    /// @return Optional containing the inverse matrix if invertible, or std::nullopt if singular.
    [[nodiscard]] std::optional<Matrix<Rows, Cols>> tryInverse() const noexcept
    {
        static_assert(Rows == Cols, "Matrix must be square to invert.");
        constexpr std::size_t N = Rows;

        std::array<std::array<double, 2U * N>, N> aug {};
        for (std::size_t r = 0U; r < N; ++r) {
            for (std::size_t c = 0U; c < N; ++c) {
                aug[r][c] = m_data[r][c];
                aug[r][c + N] = (r == c) ? 1.0 : 0.0;
            }
        }

        for (std::size_t i = 0U; i < N; ++i) {
            std::size_t maxRow = i;
            double maxVal = std::abs(aug[i][i]);
            for (std::size_t r = i + 1U; r < N; ++r) {
                if (std::abs(aug[r][i]) > maxVal) {
                    maxVal = std::abs(aug[r][i]);
                    maxRow = r;
                }
            }

            if (maxVal < 1e-12) {
                return std::nullopt;
            }

            if (maxRow != i) {
                std::swap(aug[i], aug[maxRow]);
            }

            const double pivot = aug[i][i];
            for (std::size_t c = 0U; c < 2U * N; ++c) {
                aug[i][c] /= pivot;
            }

            for (std::size_t r = 0U; r < N; ++r) {
                if (r != i) {
                    const double factor = aug[r][i];
                    for (std::size_t c = 0U; c < 2U * N; ++c) {
                        aug[r][c] -= factor * aug[i][c];
                    }
                }
            }
        }

        Matrix<Rows, Cols> inv {};
        for (std::size_t r = 0U; r < N; ++r) {
            for (std::size_t c = 0U; c < N; ++c) {
                inv(r, c) = aug[r][c + N];
            }
        }
        return inv;
    }

    /// @brief Computes matrix inverse via Gauss-Jordan elimination with partial pivoting (Square matrices only).
    /// @details Falls back to returning the identity matrix if the matrix is singular (pivot < 1e-12)
    ///          to prevent hard crashes in real-time control loops. Use tryInverse() to detect singularity explicitly.
    /// @return Inverted matrix, or identity matrix if singular.
    [[nodiscard]] Matrix<Rows, Cols> inverse() const noexcept
    {
        static_assert(Rows == Cols, "Matrix must be square to invert.");
        const auto maybeInv = tryInverse();
        if (maybeInv.has_value()) {
            return *maybeInv;
        }
        return Matrix<Rows, Cols>::identity();
    }

    /// @brief Computes lower-triangular Cholesky factor L such that A = L * L^T (Square symmetric matrices only).
    /// @details Clamps non-positive diagonal residuals to 1e-12 for numerical robustness under finite-precision drift.
    /// @return Lower-triangular Cholesky factor L.
    [[nodiscard]] Matrix<Rows, Cols> cholesky() const noexcept
    {
        static_assert(Rows == Cols, "Matrix must be square for Cholesky decomposition.");
        constexpr std::size_t N = Rows;
        Matrix<Rows, Cols> L {};

        for (std::size_t i = 0U; i < N; ++i) {
            for (std::size_t j = 0U; j <= i; ++j) {
                double sum = 0.0;
                for (std::size_t k = 0U; k < j; ++k) {
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
/// @details Provides 1D indexing, Euclidean norm, dot product, and conversion from base Matrix<Dim, 1>.
template <std::size_t Dim> class Vector : public Matrix<Dim, 1U> {
public:
    /// @brief Default constructor initializing all elements to zero.
    /// @details Invokes the base Matrix<Dim, 1U> default constructor.
    constexpr Vector() noexcept
        : Matrix<Dim, 1U>()
    {
    }

    /// @brief Constructor from base Matrix<Dim, 1U>.
    /// @details Enables seamless conversion of matrix column results into Vector<Dim>.
    /// @param[in] mat Source column matrix of dimension Dim x 1.
    constexpr Vector(const Matrix<Dim, 1U>& mat) noexcept
        : Matrix<Dim, 1U>(mat)
    {
    }

    /// @brief Assignment from base Matrix<Dim, 1U>.
    /// @details Copies elements from a Dim x 1 column matrix into this vector.
    /// @param[in] mat Source column matrix of dimension Dim x 1.
    /// @return Reference to this vector.
    Vector& operator=(const Matrix<Dim, 1U>& mat) noexcept
    {
        Matrix<Dim, 1U>::operator=(mat);
        return *this;
    }

    /// @brief Constructor from flat initializer list.
    /// @details Populates up to Dim elements and zero-pads any omitted entries.
    /// @param[in] list Flat initializer list of vector elements.
    Vector(std::initializer_list<double> list) noexcept
    {
        std::size_t i = 0U;
        for (const auto& val : list) {
            if (i < Dim) {
                (*this)(i, 0U) = val;
                ++i;
            }
        }
        for (; i < Dim; ++i) {
            (*this)(i, 0U) = 0.0;
        }
    }

    /// @brief Mutable 1D element access operator.
    /// @details Accesses element i of the column vector (clamped to valid bounds).
    /// @param[in] i Element index [0, Dim - 1].
    /// @return Reference to element i.
    [[nodiscard]] constexpr double& operator()(std::size_t i) noexcept
    {
        return Matrix<Dim, 1U>::operator()(i, 0U);
    }

    /// @brief Const 1D element access operator.
    /// @details Accesses element i of the column vector (clamped to valid bounds).
    /// @param[in] i Element index [0, Dim - 1].
    /// @return Const reference to element i.
    [[nodiscard]] constexpr const double& operator()(std::size_t i) const noexcept
    {
        return Matrix<Dim, 1U>::operator()(i, 0U);
    }

    /// @brief Mutable 1D bracket indexing operator.
    /// @details Accesses element i of the column vector (clamped to valid bounds).
    /// @param[in] i Element index [0, Dim - 1].
    /// @return Reference to element i.
    [[nodiscard]] constexpr double& operator[](std::size_t i) noexcept
    {
        return Matrix<Dim, 1U>::operator()(i, 0U);
    }

    /// @brief Const 1D bracket indexing operator.
    /// @details Accesses element i of the column vector (clamped to valid bounds).
    /// @param[in] i Element index [0, Dim - 1].
    /// @return Const reference to element i.
    [[nodiscard]] constexpr const double& operator[](std::size_t i) const noexcept
    {
        return Matrix<Dim, 1U>::operator()(i, 0U);
    }

    /// @brief Mutable 2D element access operator (inherited from Matrix).
    using Matrix<Dim, 1U>::operator();

    /// @brief Computes Euclidean L2 norm (magnitude) of the vector.
    /// @details Returns sqrt(sum_i v_i^2).
    /// @return Euclidean norm.
    [[nodiscard]] double norm() const noexcept
    {
        double sumSq = 0.0;
        for (std::size_t i = 0U; i < Dim; ++i) {
            const double val = (*this)(i);
            sumSq += val * val;
        }
        return std::sqrt(sumSq);
    }

    /// @brief Computes dot product with another vector of identical dimension.
    /// @details Returns sum_i (this_i * other_i).
    /// @param[in] other Right-hand vector.
    /// @return Scalar inner product.
    [[nodiscard]] double dot(const Vector<Dim>& other) const noexcept
    {
        double sum = 0.0;
        for (std::size_t i = 0U; i < Dim; ++i) {
            sum += (*this)(i)*other(i);
        }
        return sum;
    }

    /// @brief Computes outer product with another vector: (Dim x 1) * (1 x OtherDim) -> (Dim x OtherDim).
    /// @details Produces a matrix where element (r, c) = this(r) * other(c).
    /// @param[in] other Right-hand vector of dimension OtherDim.
    /// @return Outer product matrix of dimension Dim x OtherDim.
    template <std::size_t OtherDim>
    [[nodiscard]] Matrix<Dim, OtherDim> outer(const Vector<OtherDim>& other) const noexcept
    {
        return (*this) * other.transpose();
    }
};

/// @brief Left-hand scalar multiplication (scalar * Matrix).
/// @details Scales every element of the matrix by scalar.
/// @param[in] scalar Multiplier value.
/// @param[in] mat Right-hand matrix.
/// @return Scaled matrix of dimension Rows x Cols.
template <std::size_t Rows, std::size_t Cols>
[[nodiscard]] Matrix<Rows, Cols> operator*(double scalar, const Matrix<Rows, Cols>& mat) noexcept
{
    return mat * scalar;
}

} // namespace Math
