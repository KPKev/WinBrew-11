# SPDX-License-Identifier: GPL-3.0-or-later
import sys,zipfile
from pathlib import Path
sys.path.insert(0,str(Path('build/office-host/python-deps').resolve()))
import openpyxl
p=Path('build/office-host/xlsx/edited.xlsx')
w=openpyxl.load_workbook(p)
assert w.sheetnames==['Budget & Notes','Second Sheet','Array formulas','Protected']
s=w.worksheets[0]
assert s['B2'].value==25 and s['B2'].number_format=='$#,##0.00'
assert s['A1'].font.bold and s['B4'].value=='=SUM(B2:B3)'
assert s['B5'].value=='=IF(B4>10,42,0)'
assert s['C1'].value=='=literal text' and s['C1'].data_type=='s'
assert s['Z100'].value=='New final cell'
assert s['AA101'].value=='Outside viewport - preserve me'
assert len(s._charts)==1 and w.worksheets[3].protection.sheet
assert w.worksheets[1]['B1'].value=='Second-sheet edit'
with zipfile.ZipFile('tests/fixtures/office-interop.xlsx') as before,zipfile.ZipFile(p) as after:
 assert after.testzip() is None
 assert set(before.namelist())==set(after.namelist())
 for name in before.namelist():
  if name not in {'xl/worksheets/sheet1.xml','xl/worksheets/sheet2.xml','xl/workbook.xml'}:
   assert before.read(name)==after.read(name),name
out=openpyxl.load_workbook('build/office-host/xlsx/converted.xlsx')
assert out.worksheets[0]['B3'].value=='=B2*3'
assert out.worksheets[1]['A1'].value=='Added worksheet'
assert out.calculation.fullCalcOnLoad
print('PASS: independent OpenPyXL reads edited/exported XLSX; formulas, styling, chart, long/outside cells and protection preserved. Every untouched package part matches original bytes.')
