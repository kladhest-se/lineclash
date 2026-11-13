#!/usr/bin/env python3
"""Verify integrity and basic structure of a downloaded LineClash replay.
This verifies recorded evidence, not deterministic simulation or cheat-free play.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re


def verify(document):
    match=document['match'];raw=document['replay_source']
    if not isinstance(raw,str) or len(raw.encode())>1500000: raise ValueError('Missing or oversized replay')
    if hashlib.sha256(raw.encode()).hexdigest()!=match['replay_sha256']: raise ValueError('Replay checksum mismatch')
    events=json.loads(raw)
    if not isinstance(events,list) or len(events)>2048: raise ValueError('Invalid event count')
    if events!=document.get('events'): raise ValueError('Parsed events differ from checksum source bytes')
    previous=-1;snapshots=0;ended=False
    for event in events:
        ms=event['elapsed_ms'];line=event['line']
        if type(ms) is not int or ms<previous or ms>604800000: raise ValueError('Invalid event order/time')
        if not isinstance(line,str) or len(line.encode())>511 or any(ord(c)<32 for c in line): raise ValueError('Invalid event text')
        if not re.match(r'^(newgame |roundid |f |sb |playerlost |playerwon |pause |endgame$|playerjoin |lockdelay )',line): raise ValueError('Non-gameplay event')
        if line.startswith('f '):
            if not re.fullmatch(r'f [1-8] [0-9a-z]{264}',line): raise ValueError('Invalid board snapshot')
            snapshots+=1
        ended|=line=='endgame';previous=ms
    partial=bool(int(match.get('replay_truncated',0))) or not ended
    return dict(events=len(events),snapshots=snapshots,duration_ms=max(0,previous),partial=partial)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('file',type=Path);args=parser.parse_args()
    try:
        if args.file.stat().st_size>4000000:raise ValueError('Oversized replay file')
        result=verify(json.loads(args.file.read_text()))
    except (OSError,ValueError,KeyError,TypeError) as e:parser.exit(1,'Replay rejected: '+str(e)+'\n')
    print('Integrity and trace structure passed: '+json.dumps(result))
    print('Server-reported evidence; does not certify cheat-free play.')
if __name__=='__main__':main()
