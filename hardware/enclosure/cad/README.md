# CAD

```bash
pip install build123d pillow            # build123d 0.13
python peak.py                          # full model: STEP / STL / 3MF -> ../out/
python test_coupons.py                  # coupons -> ../coupons/
cd render && npm i && cd ..             # once; needs Playwright + Chromium
python render_all.py                    # review renders -> ../out/*.png
```

All numbers live in `params.py`. `PLACEHOLDER` = not yet measured. `COUPON` = tune after the test prints.
