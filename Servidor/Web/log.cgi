t <html>
t <head>
t <title>Log de eventos</title>
t </head>
t <body bgColor=#f4f8fb leftMargin=0 topMargin=10 marginwidth="0" marginheight="0">
i pg_header.inc
t <h2 align="center"><br>Log de eventos</h2>
t <p>
t <font size="2">
t En esta pagina se muestran los ultimos eventos guardados en la memoria no volatil del sistema.
t El maximo es de 15 eventos. Cuando se llena, se sobrescribe el mas antiguo.
t </font>
t </p>
t <table border=0 width=99%>
t <tr style="background-color: #aaccff">
t  <th width=20%>Fecha</th>
t  <th width=20%>Hora</th>
t  <th width=40%>Evento</th>
t  <th width=20%>Origen</th>
t </tr>
c z c  %s
t </table>
t <br>
t <p align=center>
c z d  %s
t </p>
t <form action="log.cgi" method="post" name="log">
t <p align=center>
t  <input type="submit" name="borrar_logs" value="Borrar log">
t  <input type="button" value="Actualizar" onclick="window.location.reload()">
t  <input type="button" value="Volver" onclick="window.location.href='comedero.cgi'">
t </p>
t </form>
i pg_footer.inc
.