# Test fixtures

Captured responses from the real services, used by `bimfo_tests` and `mock_server.py`.
`python host/live_check.py --save` refreshes `wl_westbahnhof_diva.json`.

| File | Source |
|---|---|
| `wl_*.json` | Wiener Linien realtime API (`/ogd_realtime/monitor`) |
| `wl_haltestellen.csv` | Wiener Linien station list, unchanged |
| `oebb_*.txt` | ÖBB Scotty station board and station search |

Wiener Linien data: Stadt Wien – data.wien.gv.at, CC BY 4.0. The ÖBB responses come from an
unofficial interface without an open licence and are kept only to test the parser; see
[THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md).
