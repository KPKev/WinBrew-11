# SPDX-License-Identifier: GPL-3.0-or-later
import sys,datetime
from pathlib import Path
sys.path.insert(0,str(Path('build/office-host/python-deps').resolve()))
import xlsxwriter
p=Path('tests/fixtures/office-interop.xlsx')
w=xlsxwriter.Workbook(str(p));s=w.add_worksheet('Budget & Notes');two=w.add_worksheet('Second Sheet');arr=w.add_worksheet('Array formulas');protected=w.add_worksheet('Protected')
bold=w.add_format({'bold':True,'font_color':'#217346','bg_color':'#E2F0D9'})
money=w.add_format({'num_format':'$#,##0.00'})
s.write('A1','Product',bold);s.write('B1','Price',bold);s.write('A2','Tea & <cake>');s.write('B2',12.5,money);s.write('A3','Coffee');s.write('B3',7.5,money);s.write_formula('B4','=SUM(B2:B3)',money,20)
s.write_formula('B5','=IF(B4>10,42,0)',None,42);s.write_string('C1','=literal text');s.write_string('C2','_x0041_ literal');s.write_string('C3','cafÃ© â‚¬');s.write_string('C4','L'*1000)
s.write_string('C5','binary\x00text')
s.write_rich_string('D1',bold,'Rich',' text');s.write_datetime('D2',datetime.datetime(2026,11,7),w.add_format({'num_format':'yyyy-mm-dd'}));s.write('AA101','Outside viewport - preserve me');s.write_url('E1','https://example.com',string='A link')
chart=w.add_chart({'type':'column'});chart.add_series({'values':"='Budget & Notes'!$B$2:$B$3"});s.insert_chart('G2',chart)
two.write('A1','Other worksheet');two.write_formula('A2',"='Budget & Notes'!B4",None,20)
arr.write_array_formula('A1:B2','=TRANSPOSE({1,2;3,4})');protected.write('A1','Locked');protected.protect('test-only')
w.set_properties({'title':'Synthetic WinBrew XLSX interoperability fixture'})
w.close()
print('Generated synthetic XLSX fixture with compressed shared strings, styles, chart, four sheets and array/protection cases.')
