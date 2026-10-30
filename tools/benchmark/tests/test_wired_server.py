from pathlib import Path
import tempfile,subprocess,os,json,unittest
SERVER=Path(__file__).resolve().parents[1]/'AirportItlwm_IPERF3_WIRED_SERVER.sh'
MOCK='''#!/usr/bin/env python3
import os,sys,json
from pathlib import Path
n=Path(sys.argv[0]).name;a=sys.argv[1:];wire=os.getenv('MOCK_WIRE','en5')
if n=='networksetup':
 if a==['-listallhardwareports']:print('Hardware Port: Wi-Fi\\nDevice: en0\\n\\nHardware Port: USB 10/100/1000 LAN\\nDevice: '+wire)
 elif a==['-getairportpower','en0']:print('Wi-Fi Power (en0): '+os.getenv('MOCK_POWER','Off'))
 else:raise SystemExit('Unexpected/read-write networksetup '+repr(a))
elif n=='ipconfig':
 if os.getenv('MOCK_NOIP'):raise SystemExit(1)
 print('192.0.2.33')
elif n=='ifconfig':print(wire+': status: '+('inactive'if os.getenv('MOCK_DOWN')else'active'))
elif n=='iperf3':Path(os.environ['MOCK_ARGS']).write_text(json.dumps(a))
'''
class ServerTests(unittest.TestCase):
 def run_case(self,overrides,success):
  with tempfile.TemporaryDirectory()as t:
   p=Path(t);mock=p/'mock';mock.write_text(MOCK);mock.chmod(0o755)
   for n in ['networksetup','ipconfig','ifconfig','iperf3']:(p/n).symlink_to('mock')
   env=dict(os.environ,PATH=str(p)+':'+os.environ['PATH'],WIRED_IFACE='en5',MOCK_ARGS=str(p/'args.json'),IPERF_PORT='5201');env.update(overrides)
   r=subprocess.run(['/bin/bash',str(SERVER)],env=env,capture_output=True,text=True,timeout=5)
   self.assertEqual(r.returncode==0,success,r.stdout+r.stderr)
   self.assertEqual((p/'args.json').exists(),success)
   if success:self.assertEqual(json.loads((p/'args.json').read_text()),['-s','-B','192.0.2.33','-p',env['IPERF_PORT']])
 def test_01_normal_wired(self):self.run_case({},True)
 def test_02_other_name_port(self):self.run_case({'WIRED_IFACE':'en17','MOCK_WIRE':'en17','IPERF_PORT':'5202'},True)
 def test_03_wifi_selected(self):self.run_case({'WIRED_IFACE':'en0'},False)
 def test_04_wifi_still_on(self):self.run_case({'MOCK_POWER':'On'},False)
 def test_05_no_address(self):self.run_case({'MOCK_NOIP':'1'},False)
 def test_06_no_link(self):self.run_case({'MOCK_DOWN':'1'},False)
 def test_07_bad_port(self):self.run_case({'IPERF_PORT':'70000'},False)
 def test_08_bad_interface(self):self.run_case({'WIRED_IFACE':'awdl0'},False)
if __name__=='__main__':unittest.main(verbosity=2)
