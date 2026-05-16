t <html>
t <head>
t <title>Comedero inteligente</title>
t <script language=JavaScript type="text/javascript" src="xml_http.js"></script>
t <script language=JavaScript type="text/javascript">
t var formUpdate = new periodicObj("comedero.cgx", 1000);
t function periodicUpdateComedero() {
t   updateMultiple(formUpdate);
t   setTimeout(periodicUpdateComedero, formUpdate.period);
t }
t </script>
t </head>
t <body bgColor=#f4f8fb leftMargin=0 topMargin=10 marginwidth="0" marginheight="0" onload="periodicUpdateComedero()">
i pg_header.inc
t <h2 align="center"><br>Comedero inteligente</h2>
t <p>
t <font size="2">
t Esta pagina permite visualizar el estado del comedero y enviar ordenes al modulo cliente.
t </font>
t </p>
t <form action="comedero.cgi" method="post" name="comedero">
t <input type="hidden" value="comedero" name="pg">
t <table border=0 width=99%>
t <font size="3">
t <tr style="background-color: #aaccff">
t  <th width=35%>Parametro</th>
t  <th width=65%>Valor</th>
t </tr>
t <tr>
t  <td><img src="pabb.gif">Hora RTC:</td>
t  <td align="center">
t   <input type="text" readonly style="background-color: transparent; border: 0px"
c z 1  size="30" id="hora" value="%s">
t  </td>
t </tr>
t <tr>
t  <td><img src="pabb.gif">Fecha RTC:</td>
t  <td align="center">
t   <input type="text" readonly style="background-color: transparent; border: 0px"
c z 2  size="30" id="fecha" value="%s">
t  </td>
t </tr>
t <tr>
t  <td><img src="pabb.gif">Peso en cuenco:</td>
t  <td align="center">
t   <input type="text" readonly style="background-color: transparent; border: 0px"
c z 3  size="30" id="peso" value="%s">
t  </td>
t </tr>
t <tr>
t  <td><img src="pabb.gif">Humedad deposito:</td>
t  <td align="center">
t   <input type="text" readonly style="background-color: transparent; border: 0px"
c z 4  size="30" id="humedad" value="%s">
t  </td>
t </tr>
t <tr>
t  <td><img src="pabb.gif">Nivel / distancia:</td>
t  <td align="center">
t   <input type="text" readonly style="background-color: transparent; border: 0px"
c z 5  size="30" id="distancia" value="%s">
t  </td>
t </tr>
t <tr>
t  <td><img src="pabb.gif">Consumo:</td>
t  <td align="center">
t   <input type="text" readonly style="background-color: transparent; border: 0px"
c z 6  size="30" id="consumo" value="%s">
t  </td>
t </tr>
t <tr>
t  <td><img src="pabb.gif">Estado:</td>
t  <td align="center">
t   <input type="text" readonly style="background-color: transparent; border: 0px"
c z 7  size="30" id="estado" value="%s">
t  </td>
t </tr>
t <tr>
t  <td><img src="pabb.gif">Alerta:</td>
t  <td align="center">
t   <input type="text" readonly style="background-color: transparent; border: 0px"
c z 8  size="60" id="alerta" value="%s">
t  </td>
t </tr>
t </font>
t </table>
t <br>
t <table border=0 width=99%>
t <tr style="background-color: #aaccff">
t  <th width=35%>Control</th>
t  <th width=65%>Accion</th>
t </tr>
t <tr>
t  <td><img src="pabb.gif">Dispensacion manual:</td>
t  <td align="center">
t   <input type="submit" name="dispensar" value="Dispensar ahora">
t  </td>
t </tr>
t <tr>
t  <td><img src="pabb.gif">Dispensacion automatica:</td>
t  <td align="center">
t   <input type="checkbox" name="auto" value="on"
c z 9  %s
t   > Activada
t  </td>
t </tr>
t <tr>
t  <td><img src="pabb.gif">Hora programada 1:</td>
t  <td align="center">
t   <input type="text" name="hora1" size="5"
c z a  value="%s"
t   > formato HH:MM
t  </td>
t </tr>
t <tr>
t  <td><img src="pabb.gif">Hora programada 2:</td>
t  <td align="center">
t   <input type="text" name="hora2" size="5"
c z b  value="%s"
t   > formato HH:MM
t  </td>
t </tr>
t </table>
t <p align=center>
t  <input type="submit" name="guardar" value="Guardar configuracion">
t  <input type="button" value="Actualizar" onclick="updateMultiple(formUpdate)">
t </p>
t <p align=center>
t  <a href="log.cgi">Ver log de eventos</a>
t </p>
t </form>
i pg_footer.inc
.