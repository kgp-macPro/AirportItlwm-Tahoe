#!/usr/bin/env python3
"""Reproduce the published campaign tables from original iperf3 JSON, offline."""
import argparse,csv,hashlib,json,re,statistics,sys
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--raw-dir',type=Path,required=True)
p.add_argument('--index',type=Path,default=Path(__file__).resolve().parents[1]/'docs/results/2.0.0-campaigns.json')
p.add_argument('--csv',type=Path)
a=p.parse_args();campaigns=json.loads(a.index.read_text());allrows=[]
for c in campaigns:
 rows=[];nulls=0
 for rel,sha in sorted(c['raw_files'].items()):
  f=a.raw_dir/c['id']/rel;blob=f.read_bytes();assert hashlib.sha256(blob).hexdigest()==sha,f
  j=json.loads(blob);assert not j.get('error'),f
  m=re.match(r'T(\d+)_(TCP_(?:TX|RX)_P[14]|UDP_(?:TX|RX)\d+)_',f.name);assert m,f
  rnd=int(Path(rel).parts[0][-2:]);n=int(m[1]);name=m[2];x=j['end']['sum_sent'];y=j['end']['sum_received'];cpu=j['end']['cpu_utilization_percent']
  row=dict(campaign=c['id'],round=rnd,test=n,name=name,file=rel,sender_mbps=x['bits_per_second']/1e6,receiver_mbps=y['bits_per_second']/1e6,retransmits=x.get('retransmits'),loss_pct=y.get('lost_percent'),lost_packets=y.get('lost_packets'),packets=y.get('packets'),host_cpu=cpu['host_total'],remote_cpu=cpu['remote_total'])
  rows.append(row)
  if name.startswith('TCP'):nulls+=sum(i.get('sum',{}).get('bytes')==0 and i.get('sum',{}).get('seconds',0)>=.9 for i in j['intervals'])
 assert len(rows)==60 and {(r['round'],r['test']) for r in rows}=={(i,j)for i in range(1,4)for j in range(1,21)}
 assert nulls==c['tcp_full_zero_sum_intervals']
 means={}
 for n in ['TCP_TX_P1','TCP_TX_P4','TCP_RX_P1','TCP_RX_P4']:
  selected=[x for x in rows if x['name']==n];mean=statistics.mean(x['receiver_mbps']for x in selected)
  assert abs(mean-c['tcp'][n]['receiver_mbps_mean'])<1e-9
  assert [x['retransmits']for x in selected]==c['tcp'][n]['retransmits_by_round']
  means[n]=round(mean,3)
 allrows+=rows;print(c['id'],means,'full TCP zero intervals',nulls)
if a.csv:
 with a.csv.open('w',newline='')as f:w=csv.DictWriter(f,fieldnames=list(allrows[0]));w.writeheader();w.writerows(allrows)
print('PASS',len(campaigns),'campaigns;',len(allrows),'original test JSON files and SHA256 checks')
