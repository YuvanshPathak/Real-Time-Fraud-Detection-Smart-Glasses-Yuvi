"""Score-level fusion: learned-weight heuristic + an AASIST-L model.

The two fail differently (spectral-energy statistics vs. learned waveform
detail), so a weighted combination may beat both. The combiner is a
class-balanced logistic regression on the two standardized scores, fit on
ASVspoof 2019 LA *dev* (the usual choice for score fusion: the models were
trained on LA train, so their train scores are over-confident). Thresholds are
then frozen on the same dev set by report.py. Nothing is fit on LA eval or
In-the-Wild.

Runs in the backend venv, from existing score files (no audio is read):
  backend\\.venv\\Scripts\\python.exe training/voice/score_fusion.py --aasist aasist-l_pretrained
"""

import argparse
import sys
from pathlib import Path

import numpy as np

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "eval"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import scorefile  # noqa: E402
from heuristic_logreg import fit_logreg  # noqa: E402

SETS = ("asvspoof2019_dev", "asvspoof2019_eval", "in_the_wild")


def load_pair(score_dir, aasist, name):
    h = scorefile.read(score_dir / f"heuristic-lr__{name}.csv")
    a = scorefile.read(score_dir / f"{aasist}__{name}.csv")
    order = {k: i for i, k in enumerate(a["id"])}
    idx = np.array([order[k] for k in h["id"]])  # align rows by clip id
    if not np.array_equal(a["is_bona"][idx], h["is_bona"]):
        raise ValueError(f"label mismatch between score files for {name}")
    return h, np.c_[h["scores"], a["scores"][idx]], a["meta"].get("method", aasist)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--aasist", required=True, help="score-file prefix, e.g. aasist-l_pretrained or aasist-l_ft_both")
    p.add_argument("--score-dir", default=str(REPO_ROOT / "eval" / "scores" / "voice"))
    args = p.parse_args()
    score_dir = Path(args.score_dir)

    data = {name: load_pair(score_dir, args.aasist, name) for name in SETS}
    dev_h, dev_x, aasist_method = data["asvspoof2019_dev"]
    mu, sd = dev_x.mean(0), dev_x.std(0) + 1e-12
    w = fit_logreg((dev_x - mu) / sd, dev_h["is_bona"].astype(float))
    print("weights (bias, heuristic-lr, aasist):", np.round(w, 3))

    method = f"Fusion: heuristic-LR + {aasist_method}"
    for name, (h, x, _) in data.items():
        scores = np.c_[np.ones(len(x)), (x - mu) / sd] @ w
        rows = [{k: h[k][i] for k in scorefile.COLUMNS if k not in ("score", "label")}
                | {"label": "bonafide" if h["is_bona"][i] else "attack", "score": f"{scores[i]:.6f}"}
                for i in range(len(x))]
        out = score_dir / f"fusion-lr+{args.aasist}__{name}.csv"
        scorefile.write(out, rows, dict(
            module="voice", method=method, model="logistic regression on [heuristic-lr score, AASIST-L score]",
            trained_on="asvspoof2019_la_dev", weights=[round(float(v), 6) for v in w], dataset=name))
        print(f"wrote {out.name}")


if __name__ == "__main__":
    main()
