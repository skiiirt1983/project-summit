# Third-party notices

| Component | Version | Licence | Where | Used for |
| --- | --- | --- | --- | --- |
| cJSON (Dave Gamble and contributors) | 1.7.18 | MIT (`third_party/cjson/LICENSE`) | `third_party/cjson/` | Host builds only (host tests, PC simulator). The device uses ESP-IDF's bundled `json` component. |
| LVGL | 9.4.0 | MIT | fetched at build time (ESP component registry / FetchContent) | UI |
| ESP-IDF | 5.5.x | Apache-2.0 | toolchain | Firmware framework |
| pdf.js (Mozilla) | 4.10.38 (`pdfjs-dist`) | Apache-2.0 (`firmware/web/page/vendor/pdfjs/LICENSE`), GPL-3.0 compatible | `firmware/web/page/vendor/pdfjs/` | Runs in the browser, served by the device: converts recipe PDFs to page images and text during import |
