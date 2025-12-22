import numpy as np
from PIL import Image


def load_image(path):
    img = Image.open(path).convert("RGB")
    arr = np.asarray(img, dtype=np.float32)
    return arr / 255.0


def auto_crop_black_borders(arr, threshold=0.02):
    """Remove columns that are nearly black across all rows."""
    h, w, _ = arr.shape
    col_energy = np.max(arr, axis=(0, 2))
    cols = np.where(col_energy > threshold)[0]
    if cols.size == 0:
        return arr
    left, right = cols[0], cols[-1] + 1
    return arr[:, left:right]


def resize_to_match(arr, target_shape):
    from PIL import Image

    target_h, target_w = target_shape[:2]
    if arr.shape[0] == target_h and arr.shape[1] == target_w:
        return arr
    pil = Image.fromarray(np.clip(arr * 255.0, 0, 255).astype(np.uint8))
    pil = pil.resize((target_w, target_h), Image.BICUBIC)
    return np.asarray(pil, dtype=np.float32) / 255.0


def compute_psnr(ref, test):
    mse = np.mean((ref - test) ** 2)
    if mse == 0:
        return float('inf')
    return 10.0 * np.log10(1.0 / mse)


def to_ycbcr(arr):
    # BT.601
    R, G, B = arr[..., 0], arr[..., 1], arr[..., 2]
    Y = 0.299 * R + 0.587 * G + 0.114 * B
    Cb = -0.168736 * R - 0.331264 * G + 0.5 * B + 0.5
    Cr = 0.5 * R - 0.418688 * G - 0.081312 * B + 0.5
    return np.stack([Y, Cb, Cr], axis=-1)


def to_hsv(arr):
    from colorsys import rgb_to_hsv
    flat = arr.reshape(-1, 3)
    hsv = np.array([rgb_to_hsv(*pixel) for pixel in flat], dtype=np.float32)
    return hsv.reshape(arr.shape)


def summarize_channel(name, ref, test):
    ref_mean = np.mean(ref)
    test_mean = np.mean(test)
    ref_std = np.std(ref)
    test_std = std = np.std(test)
    diff_mean = np.mean(test - ref)
    diff_std = np.std(test - ref)
    print(f"{name}: ref_mean={ref_mean:.4f}, test_mean={test_mean:.4f}, "
          f"ref_std={ref_std:.4f}, test_std={test_std:.4f}, "
          f"diff_mean={diff_mean:.4f}, diff_std={diff_std:.4f}")


def main():
    generated = load_image("cpp_chroma_het_viewable.png")
    baseline = load_image("cpp-prototype/frame_pal_chroma_ar43_1_out1.tbc.png")

    generated_cropped = auto_crop_black_borders(generated)
    baseline_cropped = auto_crop_black_borders(baseline)

    min_h = min(generated_cropped.shape[0], baseline_cropped.shape[0])
    min_w = min(generated_cropped.shape[1], baseline_cropped.shape[1])

    generated_aligned = generated_cropped[:min_h, :min_w]
    baseline_aligned = baseline_cropped[:min_h, :min_w]

    if generated_aligned.shape != baseline_aligned.shape:
        generated_aligned = resize_to_match(generated_aligned, baseline_aligned.shape)

    diff = generated_aligned - baseline_aligned

    print("== Basic Stats ==")
    print(f"Shape: {generated_aligned.shape}")
    print(f"Mean abs diff: {np.mean(np.abs(diff)):.4f}")
    print(f"RMSE: {np.sqrt(np.mean(diff ** 2)):.4f}")
    print(f"PSNR: {compute_psnr(baseline_aligned, generated_aligned):.2f} dB")

    # Per-channel stats
    print("\n== RGB Channel Stats ==")
    channel_names = ["R", "G", "B"]
    for i, name in enumerate(channel_names):
        summarize_channel(name, baseline_aligned[..., i], generated_aligned[..., i])

    # Saturation / Hue
    baseline_hsv = to_hsv(baseline_aligned)
    generated_hsv = to_hsv(generated_aligned)

    print("\n== HSV Stats ==")
    summarize_channel("Hue", baseline_hsv[..., 0], generated_hsv[..., 0])
    summarize_channel("Saturation", baseline_hsv[..., 1], generated_hsv[..., 1])
    summarize_channel("Value", baseline_hsv[..., 2], generated_hsv[..., 2])

    # YCbCr analysis
    baseline_ycbcr = to_ycbcr(baseline_aligned)
    generated_ycbcr = to_ycbcr(generated_aligned)
    print("\n== YCbCr Stats ==")
    for i, name in enumerate(["Y", "Cb", "Cr"]):
        summarize_channel(name, baseline_ycbcr[..., i], generated_ycbcr[..., i])

    # Noise estimate: compute std of high-pass filtered difference (simple)
    kernel = np.array([[0, -1, 0], [-1, 4, -1], [0, -1, 0]], dtype=np.float32)
    from scipy.signal import convolve2d

    hp_diff = np.stack([
        convolve2d(diff[..., c], kernel, mode="valid", boundary="symm")
        for c in range(3)
    ], axis=-1)
    noise_std = np.std(hp_diff)
    print(f"\nNoise estimate (Laplacian std of diff): {noise_std:.4f}")

    # Save difference visualization
    diff_vis = np.clip((diff * 0.5 + 0.5) * 255.0, 0, 255).astype(np.uint8)
    Image.fromarray(diff_vis).save("image_diff_visual.png")
    print("Saved diff visualization to image_diff_visual.png")


if __name__ == "__main__":
    main()
