# Native chart formatting check

Run `sh native/client/tchart/check.sh` to rebuild and link the real TChart and
TCML libraries, then exercise native `CTChart::Format` with ASan/UBSan and
non-POD variadic arguments treated as errors. No copied formatting implementation
is tested. The original Windows view is unchanged. Windows build NOT TESTED.
See `research/archive/c4/c4-q-c14.md` for the C4 compile measurement and remaining limits.
