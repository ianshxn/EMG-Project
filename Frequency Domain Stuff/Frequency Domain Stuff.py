import numpy as np
import pandas as pd
from scipy.signal import welch
import matplotlib.pyplot as plt

def infer_fs_from_time_us(t_us: np.ndarray) -> float:
    """Infer sampling rate from Time(us) column using median time step."""
    dt_us = np.diff(t_us.astype(float))
    dt_us = dt_us[np.isfinite(dt_us) & (dt_us > 0)]
    if len(dt_us) < 10:
        raise ValueError("Not enough valid timestamps to infer fs.")
    dt_s = np.median(dt_us) * 1e-6
    return 1.0 / dt_s

#---- Frequency domain feature functions ----

def mean_frequency(f: np.ndarray, P: np.ndarray, eps: float = 1e-12) -> float:
    return float(np.sum(f * P) / (np.sum(P) + eps))

def median_frequency(f: np.ndarray, P: np.ndarray) -> float:
    c = np.cumsum(P)
    half = c[-1] / 2.0
    idx = np.searchsorted(c, half)
    if idx <= 0:
        return float(f[0])
    if idx >= len(f):
        return float(f[-1])
    f0, f1 = f[idx - 1], f[idx]
    c0, c1 = c[idx - 1], c[idx]
    if c1 == c0:
        return float(f1)
    return float(f0 + (half - c0) * (f1 - f0) / (c1 - c0))

def band_mask(f: np.ndarray, low: float, high: float) -> np.ndarray:
    return (f >= low) & (f <= high)

def spectral_moment(f: np.ndarray, P: np.ndarray, k: int, eps: float = 1e-12) -> float:
    return float(np.sum((f ** k) * P) / (np.sum(P) + eps))

def smr(f: np.ndarray, P: np.ndarray, k_num: int = 2, k_den: int = 1, eps: float = 1e-12) -> float:
    m_num = spectral_moment(f, P, k_num, eps=eps)
    m_den = spectral_moment(f, P, k_den, eps=eps)
    return float(m_num / (m_den + eps))

def bandpower(f: np.ndarray, P: np.ndarray, low: float, high: float) -> float:
    m = (f >= low) & (f <= high)
    if not np.any(m):
        return 0.0
    return float(np.trapezoid(P[m], f[m]))   # integral 

def power_ratio(f: np.ndarray, P: np.ndarray, num_band: tuple[float, float], den_band: tuple[float, float], eps: float = 1e-12) -> float:
    p_num = bandpower(f, P, num_band[0], num_band[1])
    p_den = bandpower(f, P, den_band[0], den_band[1])
    return float(p_num / (p_den + eps))

def spectral_entropy(f: np.ndarray, P: np.ndarray, low: float, high: float, eps: float = 1e-12) -> float:
    m = (f >= low) & (f <= high)
    Pb = P[m]
    if Pb.size == 0:
        return 0.0
    ps = Pb / (np.sum(Pb) + eps)              # normalize PSD -> "probabilities"
    H = -np.sum(ps * np.log(ps + eps))        # Shannon entropy (nats)
    H_norm = H / (np.log(len(ps) + eps))      # normalize to 0..1
    return float(H_norm)

# Time domain features functions:

def rms(x: np.ndarray) -> float:
    return float(np.sqrt(np.mean(x ** 2)))

def mav(x: np.ndarray) -> float:
    return float(np.mean(np.abs(x)))

def waveform_length(x: np.ndarray) -> float:
    return float(np.sum(np.abs(np.diff(x))))

def zero_crossings(x: np.ndarray, threshold: float = 0.0) -> int:
    x = np.asarray(x)
    s = np.sign(x - threshold)
    s[s == 0] = -1
    return int(np.sum(s[:-1] != s[1:]))


        



csv_path = r"C:\Users\ishen\Downloads\EMG_RAW_07.csv"   ### CHANGE
time_col = "Time(us)"          
sig_col  = "Filtered"          

df = pd.read_csv(csv_path)
t_us = df[time_col].to_numpy()
x = df[sig_col].to_numpy(dtype=float)

fs = infer_fs_from_time_us(t_us)
print(f"Inferred fs = {fs:.2f} Hz")
x = x - np.mean(x)

# Welch PSD over the entire file 
# 2kHz: 512
# 1kHz: 256
if fs >= 1500:
    nperseg = 1024
else:
    nperseg = 512

f_all, P_all = welch(
    x, fs=fs, window="hann",
    nperseg=min(nperseg, len(x)),
    noverlap=min(nperseg // 2, max(0, len(x) - 1)),
    detrend="constant", scaling="density"
)

band_low, band_high = 20.0, 450.0
m = band_mask(f_all, band_low, band_high)
f_band, P_band = f_all[m], P_all[m]

mnf_all = mean_frequency(f_band, P_band)
mdf_all = median_frequency(f_band, P_band)
peak_all = float(f_band[np.argmax(P_band)])
total_power_all = float(np.sum(P_band))

print("Whole-recording PSD features (20–450 Hz):")
print(f"  MNF  = {mnf_all:.2f} Hz")
print(f"  MDF  = {mdf_all:.2f} Hz")
print(f"  Peak = {peak_all:.2f} Hz")
print(f"  Power= {total_power_all:.6g}")

# Save whole PSD to CSV
pd.DataFrame({"freq_hz": f_all, "psd": P_all}).to_csv(
    csv_path.replace(".csv", "_welch_psd.csv"), index=False
)




# Sliding-window Welch PSD + features over time 
window_s = 0.25     # 250 ms windows
overlap = 0.75       # 75  % overlap
win_len = int(round(window_s * fs))
hop = max(int(round(win_len * (1 - overlap))), 1)

# Welch inside each window:
# Keep it comparable across fs by keeping nperseg/fs similar.
if fs >= 1500:
    nperseg_w = 512     # ~0.256 s at 2k
else:
    nperseg_w = 256     # ~0.256 s at 1k
noverlap_w = nperseg_w // 2

rows = []
for start in range(0, len(x) - win_len + 1, hop):
    seg = x[start:start + win_len]

    f, P = welch(
        seg, fs=fs, window="hann",
        nperseg=min(nperseg_w, len(seg)),
        noverlap=min(noverlap_w, max(0, len(seg) - 1)),
        detrend="constant", scaling="density"
    )

    mb = band_mask(f, band_low, band_high)
    fb, Pb = f[mb], P[mb]
    power = float(np.sum(Pb))
    # Spectral moment ratios
    smr_21 = smr(fb, Pb, k_num=2, k_den=1)   # SMR = m2/m1

    if power <= 0 or np.allclose(Pb, 0):
        mnf = mdf = peak = 0.0
    else:
        mnf = mean_frequency(fb, Pb)
        mdf = median_frequency(fb, Pb)
        peak = float(fb[np.argmax(Pb)])

    # Power ratios
    p_low = (20, 60)
    p_mid = (60, 150)
    p_high = (150, 450)
    p_EMG = (20, 450)

    #spectral entropy
    spec_ent = spectral_entropy(f, P, 20.0, 450.0)

    LHRatio = power_ratio(f, P, p_low, p_high)
    LMRatio = power_ratio(f, P, p_low, p_mid) 
    HTRatio = power_ratio(f, P, p_high, p_EMG)

    #Time series features
    rms_val = rms(seg)
    mav_val = mav(seg)
    wl_val = waveform_length(seg)

    thr = 0.01 * np.std(seg)
    ZC_val = zero_crossings(seg, threshold=thr)

    t_center_s = (t_us[start] * 1e-6) + (0.5 * win_len / fs)
    rows.append([t_center_s, mnf, mdf, peak, power, smr_21, LHRatio, LMRatio, HTRatio, 
                 spec_ent, rms_val, mav_val, wl_val, ZC_val])

feat_df = pd.DataFrame(rows, columns=[
    "t_center_s", "mnf_hz", "mdf_hz", "peak_hz", "band_power", "smr_21", 
    "LHRatio", "LMRatio", "HTRatio", "spec_ent", "rms", "mav", "wl", "ZC"
])
feat_df.to_csv(csv_path.replace(".csv", "_freq_features.csv"), index=False)
print(f"Wrote: {csv_path.replace('.csv', '_freq_features.csv')}")

# ---- Optional plots ----
do_plots = True
if do_plots:
    # PSD plot (whole recording)
    plt.figure()
    plt.plot(f_band, P_band)
    plt.xlabel("Frequency (Hz)")
    plt.ylabel("PSD (V^2/Hz or ADC^2/Hz)")
    plt.title("Welch PSD (20–450 Hz)")
    plt.show()

    # MNF/MDF over time
    plt.figure()
    plt.plot(feat_df["t_center_s"], feat_df["mnf_hz"], label="MNF")
    plt.plot(feat_df["t_center_s"], feat_df["mdf_hz"], label="MDF")
    plt.xlabel("Time (s)")
    plt.ylabel("Frequency (Hz)")
    plt.title("Frequency Features Over Time")
    plt.legend()
    plt.show()
