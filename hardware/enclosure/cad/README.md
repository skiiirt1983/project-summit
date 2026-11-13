# CAD

```bash
pip install build123d pillow python-docx   # build123d 0.13
python peak.py                             # full model: STEP / STL / 3MF -> ../out/
python test_coupons.py                     # pieces still to print -> ../coupons/reprint-*.3mf
cd render && npm i && cd ..                # once; needs Playwright + Chromium
python render_all.py                       # review renders -> ../out/*.png
python test_pictures.py                    # pictures for the checklist -> ../tests/
python remaining_tests.py                  # ../REMAINING-TESTS.md and .docx
```

All numbers live in `params.py`. `PLACEHOLDER` = not yet measured. `COUPON` = tune after the test prints.
