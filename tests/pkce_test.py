"""Run with: python tests/pkce_test.py

Checks tools/spotify_login.py's PKCE code_challenge derivation against the
published RFC 7636 Appendix B test vector. No network required.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from spotify_login import code_challenge_from_verifier  # noqa: E402

RFC7636_VERIFIER = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"
RFC7636_CHALLENGE = "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM"


def main() -> int:
    actual = code_challenge_from_verifier(RFC7636_VERIFIER)
    if actual != RFC7636_CHALLENGE:
        print(f"FAIL: code_challenge_from_verifier(RFC7636_VERIFIER) = {actual!r}; "
              f"expected {RFC7636_CHALLENGE!r}", file=sys.stderr)
        return 1

    print("PASS: code_challenge_from_verifier matches RFC 7636 Appendix B.1 test vector")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
