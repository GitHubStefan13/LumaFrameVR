import re
import signal
import subprocess
import sys
probe, *runner = sys.argv[1:]
for value in (signal.SIGSEGV, signal.SIGABRT, signal.SIGBUS):
    result = subprocess.run([*runner, probe, str(value)], text=True, capture_output=True, timeout=10)
    assert result.returncode in (-value, 128 + value), (value, result.returncode, result.stderr)
    assert f'[LumaFrameVR fatal] signal={value} ' in result.stderr, result.stderr
    assert re.search(r'pc=0x[0-9a-f]+ fault=0x[0-9a-f]+', result.stderr), result.stderr
    assert '[LumaFrameVR launch environment]' in result.stderr, result.stderr
    print(f'PASS signal={value}: fatal marker captured; original signal termination preserved')
