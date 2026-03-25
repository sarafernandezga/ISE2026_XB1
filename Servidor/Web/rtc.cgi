t <html><head><title>RTC</title>
t <script language=JavaScript type="text/javascript" src="xml_http.js"></script>
t <script language=JavaScript type="text/javascript">
t var formUpdate = new periodicObj("rtc.cgx", 1000);
t function periodicUpdateRTC() {
t   updateMultiple(formUpdate);
t   setTimeout(periodicUpdateRTC, formUpdate.period);
t }
t </script></head>
t <body onload="periodicUpdateRTC()">
i pg_header.inc
t <h2 align="center"><br>RTC del sistema</h2>
t <p><font size="2">
t  Esta pagina muestra automaticamente la fecha y la hora actuales
t  almacenadas en el RTC de la placa.
t </font></p>
t <form action="rtc.cgi" method="post" name="rtc">
t <input type="hidden" value="rtc" name="pg">
t <table border=0 width=99%><font size="3">
t <tr style="background-color: #aaccff">
t  <th width=30%>Parametro</th>
t  <th width=70%>Valor</th></tr>
t <tr>
t  <td><img src="pabb.gif">Hora actual:</td>
t  <td align="center">
t   <input type="text" readonly style="background-color: transparent; border: 0px"
c h 1  size="20" id="rtc_time" value="%s"></td>
t </tr>
t <tr>
t  <td><img src="pabb.gif">Fecha actual:</td>
t  <td align="center">
t   <input type="text" readonly style="background-color: transparent; border: 0px"
c h 2  size="20" id="rtc_date" value="%s"></td>
t </tr>
t </font></table>
t <p align=center>
t <input type=button value="Refresh" onclick="updateMultiple(formUpdate)">
t </p></form>
i pg_footer.inc
t </body></html>
.