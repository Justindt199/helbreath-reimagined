"""Replace plaintext passwords in old server logs with the same salted hash
the servers now write (Source/Shared/LogHash.h, LogPw()).

Run from the Server folder:  python tools/scrub_log_passwords.py
Lines already hashed (#xxxxxxxxxxxxxxxx) are left alone, so it is safe to rerun.
"""
import hashlib
import os
import re


def h(p):
    return '#' + hashlib.sha256(('HelbreathReimagined-log:' + p[:63]).encode('latin-1')).hexdigest()[:16]


def keep_or_hash(x):
    return x if (not x or x.startswith('#')) else h(x)


SINGLE = [
    re.compile(r'(Acc\([^ /)]* / )([^#)][^)]*?)(\))'),                  # Sent confirmed Login
    re.compile(r'(Confirmed client: \S+ \([^ /]* / )([^#][^ ]*?)( - )'),
    re.compile(r'(Logins list: \S+ \S+ )([^# ]\S*)( )'),
    re.compile(r'(Correct Password\[ )([^#\]][^\]]*?)( \])'),
    re.compile(r'(Password received\[ )([^#\]][^\]]*?)( \])'),
]
ACCPWD = re.compile(r'(AccPwd\[)([^\]]*)(\]\[)([^\]]*)(\])')
PWD = re.compile(r'(pwd\[)([^\]]*)(\])')


def scrub(s):
    for r in SINGLE:
        s = r.sub(lambda m: m.group(1) + h(m.group(2)) + m.group(3), s)
    s = ACCPWD.sub(lambda m: m.group(1) + keep_or_hash(m.group(2)) + m.group(3)
                   + keep_or_hash(m.group(4)) + m.group(5), s)
    s = PWD.sub(lambda m: m.group(1) + '/'.join(keep_or_hash(x) for x in m.group(2).split('/'))
                + m.group(3), s)
    return s


def main(top='.'):
    n = 0
    for root, _, files in os.walk(top):
        if 'Logs' not in root:
            continue
        for f in files:
            p = os.path.join(root, f)
            with open(p, encoding='latin-1', newline='') as fh:
                s = fh.read()
            t = scrub(s)
            if t != s:
                with open(p, 'w', encoding='latin-1', newline='') as fh:
                    fh.write(t)
                n += 1
                print('scrubbed', p)
    print(n, 'files')


if __name__ == '__main__':
    main()
