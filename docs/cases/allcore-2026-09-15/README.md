# Full-core 30s stress run — vendored SDC dependency suite (2026-09-15)

Archived evidence for README.md / third-party/openblas/build.sh references to
the "all-core 30s zero-mismatch" run (128 logical cores, Kunpeng 920).

- command-line: 'sdcshield -e openblas_dgemm,openblas_sgemm,openblas_zgemm,sleef_neon,isal_igzip,pocketfft_fft,openssl_sha -t 30000'
- binary version: sdcshield-d4d1c4f37731
- total test fragments: 8806, results: {'pass': 8806}
- per-test fragment counts:
  - isal_igzip: 748
  - mce_check: 1
  - openblas_dgemm: 127
  - openblas_sgemm: 542
  - openblas_zgemm: 46
  - openssl_sha: 53
  - pocketfft_fft: 3461
  - sleef_neon: 3828

Full YAML log available in the session workspace; this summary is the durable record.
