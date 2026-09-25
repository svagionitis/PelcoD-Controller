# Gabor Transforms & Filterbanks 101: An Intuitive (ELI5) and Technical Guide

> **Target Audience:** From beginners seeking an intuitive conceptual grasp of time-frequency and space-frequency analysis to computer vision, robotics, optical inspection, and video tracking engineers implementing multi-angle sharpness assessment, directional motion blur estimation, and texture feature modeling.

---

## Executive Summary & ELI5 (Explain Like I'm 5)

A **Gabor Filter** is a specialized mathematical lens designed to answer two questions simultaneously at any point in a signal or image:
1. **"What frequency (stripe thickness / vibration pitch) is present here?"**
2. **"At what exact orientation (angle) does it point, and where exactly is it located?"**

Standard Fourier transforms tell you what frequencies exist anywhere in an image, but they have zero spatial localization (they can't tell you *where* something is). Wavelets localize transient shocks, but standard wavelets struggle with arbitrary orientation angles. 

A **Gabor Filter** achieves the theoretical maximum mathematical limit of sharpness in both space and frequency at the same time—a physical law known as the **Heisenberg-Gabor Uncertainty Principle**.

---

### The Intuitive Analogy: The Oriented Comb Over a Fabric

Imagine you are inspecting a finely woven piece of carbon fiber or corduroy fabric:

```
[ Unoriented Lens / Standard Blur ]  ──► Averages all directions equally (loses directional information)
[ 0° Gabor Filter (Vertical) ]       ──► Responds strongly to vertical stripes, blind to horizontal
[ 90° Gabor Filter (Horizontal) ]    ──► Responds strongly to horizontal stripes, blind to vertical
[ 45° Gabor Filter (Diagonal) ]      ──► Responds strongly to 45° diagonal weave patterns
```

1. **The Sinusoidal Carrier (The Comb):**
   - The filter has parallel ridges and troughs like a comb with a specific spacing (wavelength $\lambda$). It only vibrates when ridges in the image match its spacing.
2. **The Gaussian Envelope (The Spotlight):**
   - The comb is restricted inside a soft circular or elliptical Gaussian spotlight ($\sigma$). This ensures the filter only inspects a local neighborhood rather than the entire universe.
3. **The Quadrature Pair (Real and Imaginary):**
   - The filter comes in two versions shifted by 90 degrees: a **Cosine** (even/symmetric) filter that detects lines and bars, and a **Sine** (odd/antisymmetric) filter that detects edges and boundaries.
   - When you combine both using Pythagoras:
     $$\text{Energy} = \sqrt{\text{Real}^2 + \text{Imag}^2}$$
     you get the **pure directional energy**, completely independent of whether a pixel happens to be in a light stripe or a dark stripe!

---

## Mathematical Formulation

### 1. 1D Complex Gabor Wavelet

In continuous time $t$, the 1D Gabor function is a complex harmonic exponential modulated by a Gaussian bell curve:

$$g(t) = \frac{1}{\sqrt{2\pi}\sigma} \exp\left(-\frac{t^2}{2\sigma^2}\right) \exp(i 2\pi f_0 t)$$

Decomposing into real and imaginary parts:
- **Even (Real / Cosine) Kernel:**
  $$g_{\text{re}}(t) = \frac{1}{\sqrt{2\pi}\sigma} \exp\left(-\frac{t^2}{2\sigma^2}\right) \cos(2\pi f_0 t)$$
- **Odd (Imaginary / Sine) Kernel:**
  $$g_{\text{im}}(t) = \frac{1}{\sqrt{2\pi}\sigma} \exp\left(-\frac{t^2}{2\sigma^2}\right) \sin(2\pi f_0 t)$$

#### The Heisenberg-Gabor Uncertainty Limit
Every time-frequency transform obeys:
$$\Delta t \cdot \Delta \omega \ge \frac{1}{2}$$
The Gaussian-windowed harmonic function is the **only function in existence** that achieves the theoretical minimum lower bound $\Delta t \cdot \Delta \omega = 1/2$.

---

### 2. 2D Spatial Gabor Filter (Quadrature Pair)

For 2D image coordinates $(x, y)$, rotated spatial coordinates $(x', y')$ are defined by the orientation angle $\theta$:

$$x' = x \cos\theta + y \sin\theta$$
$$y' = -x \sin\theta + y \cos\theta$$

The 2D complex Gabor kernel is:

$$g(x, y; \lambda, \theta, \psi, \sigma, \gamma) = \exp\left(-\frac{x'^2 + \gamma^2 y'^2}{2\sigma^2}\right) \exp\left(i \left(2\pi \frac{x'}{\lambda} + \psi\right)\right)$$

Where:
- $\lambda$: Wavelength of the sinusoidal carrier in pixels (spatial frequency $f = 1 / \lambda$).
- $\theta$: Orientation angle in radians normal to the parallel stripe wavefront.
- $\psi$: Phase offset ($\psi = 0$ for symmetric cosine line detector, $\psi = \pi/2$ for antisymmetric sine edge detector).
- $\sigma$: Standard deviation of the Gaussian envelope determining spatial support.
- $\gamma$: Spatial aspect ratio (ellipticity, typically $0.5$ for elliptical elongation along the ridge direction).

#### DC Bias Suppression
In natural images, background illumination varies. To prevent Gabor filters from responding to flat, uniform fields, the real (even) kernel is mean-centered:
$$g'_{\text{re}}(x, y) = g_{\text{re}}(x, y) - \frac{1}{K^2} \sum_{u, v} g_{\text{re}}(u, v)$$
This guarantees that $\sum g'_{\text{re}} = 0$, giving strictly zero response on flat surfaces.

---

### 3. Gabor Energy and Phase Response

Convolving an input image $I(x, y)$ with the even ($g_{\text{re}}$) and odd ($g_{\text{im}}$) quadrature kernels produces two response maps:
$$R_{\text{re}}(x, y) = I(x, y) * g_{\text{re}}(x, y)$$
$$R_{\text{im}}(x, y) = I(x, y) * g_{\text{im}}(x, y)$$

- **Local Energy / Magnitude:**
  $$E(x, y) = \sqrt{R_{\text{re}}^2(x, y) + R_{\text{im}}^2(x, y)}$$
- **Local Phase:**
  $$\Phi(x, y) = \text{atan2}(R_{\text{im}}(x, y), R_{\text{re}}(x, y))$$

---

## The 2D Gabor Filterbank Architecture

A **Gabor Filterbank** tiles the continuous spatial-frequency plane by instantiating a grid of filters across:
- **$S$ Scales (Frequencies):** Logarithmic or octave intervals ($\lambda_s = \lambda_0 \cdot 2^s$).
- **$O$ Orientations:** Uniform angular steps ($\theta_o = o \cdot \frac{\pi}{O}$).

```
                  Orientation Angle θ
           0°        45°        90°       135°
       ┌──────────┬──────────┬──────────┬──────────┐
  λ₁   │ Scale 0  │ Scale 0  │ Scale 0  │ Scale 0  │  Fine Details (High Frequencies)
(Fine) │   0°     │   45°    │   90°    │   135°   │
       ├──────────┼──────────┼──────────┼──────────┤
  λ₂   │ Scale 1  │ Scale 1  │ Scale 1  │ Scale 1  │  Medium Textures
       │   0°     │   45°    │   90°    │   135°   │
       ├──────────┼──────────┼──────────┼──────────┤
  λ₃   │ Scale 2  │ Scale 2  │ Scale 2  │ Scale 2  │  Coarse Structures
(Coarse)│  0°     │   45°    │   90°    │   135°   │
       └──────────┴──────────┴──────────┴──────────┘
```

---

## Applications in PTZ Cameras and Tactical Surveillance

### 1. Multi-Angle Optical Sharpness & Directional Motion Blur Estimation
Standard focus evaluation metrics (such as Laplacian variance or 8x8 DCT AC sum) are **isotropic**—they output a single scalar score regardless of orientation.

When a PTZ camera rotates rapidly during a horizontal pan, the frame undergoes **directional motion blur**:
- Horizontal lines (which vary vertically, $\theta = 90^\circ$) remain sharp!
- Vertical lines (which vary horizontally, $\theta = 0^\circ$) are smeared into smooth streaks!

The Gabor filterbank computes directional sharpness:
$$S(\theta_o) = \frac{1}{S} \sum_{s=0}^{S-1} \text{MeanEnergy}(s, \theta_o)$$

From this, the **Anisotropy Index** is computed:
$$\alpha = \frac{\max(S) - \min(S)}{\max(S) + \min(S)} \in [0.0, 1.0]$$
- $\alpha \approx 0.0$: Uniform defocus blur or isotropic scene.
- $\alpha > 0.4$: High-velocity directional blur! The camera controller immediately knows the camera is panning or tilting and suppresses autofocus hunting until motion stops.

### 2. Texture Feature Extraction & Target Appearance Modeling
By computing the mean and variance of energy responses across all $S \times O$ channels:
$$\mathbf{f} = \left[ \mu_{0,0}, \sigma^2_{0,0}, \mu_{0,1}, \sigma^2_{0,1}, \dots, \mu_{S-1, O-1}, \sigma^2_{S-1, O-1} \right]^T \in \mathbb{R}^{2 \cdot S \cdot O}$$
This compact $24$-element or $32$-element vector uniquely characterizes target textures, enabling re-identification of vehicles, camouflage breaking in foliage, and pedestrian tracking across occlusions.

---

## C++17 Implementation in PelcoD-Controller

The codebase provides both pure standard library core math and OpenCV real-time video pipeline integration:

1. **`libs/Math/Gabor.h` & `libs/Math/Gabor.cpp`**:
   - [`createGaborKernel1D()`](../../libs/Math/Gabor.h#L78): Generates complex 1D analytic impulse responses.
   - [`createGaborKernel2D()`](../../libs/Math/Gabor.h#L84): Generates 2D quadrature kernel pairs with DC bias removal.
   - [`convolve2D()`](../../libs/Math/Gabor.h#L95): High-performance clamped 2D convolution.
   - [`GaborFilterBank`](../../libs/Math/Gabor.h#L115): Multi-scale, multi-orientation filterbank with ROI sharpness and feature extraction.

2. **`libs/VideoFilters/SpatialFilters.h` & `libs/VideoFilters/SpatialFilters.cpp`**:
   - [`GaborFilter`](../../libs/VideoFilters/SpatialFilters.h#L374): Real-time frame processor with `Energy`, `RealComponent`, `ImagComponent`, and `Overlay` visualization modes.

### Code Example: Evaluating Directional Sharpness

```cpp
#include "Math/Gabor.h"

// Configure a 4-orientation, 2-scale filterbank
Math::GaborFilterBankConfig config {};
config.numScales = 2U;
config.numOrientations = 4U; // 0°, 45°, 90°, 135°
config.baseWavelength = 4.0;
config.kernelSize = 21;

Math::GaborFilterBank filterBank(config);

// Evaluate incoming grayscale frame
Math::GaborFilterBankResult result = filterBank.evaluate(grayPixels, width, height);

// Check for directional motion blur
if (result.anisotropyIndex > 0.4) {
    // Camera is experiencing directional blur along dominant orientation
    double blurNormal = result.dominantOrientationRad;
}
```
