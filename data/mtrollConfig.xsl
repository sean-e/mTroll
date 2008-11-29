<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:template match="/">
	<html>
		<head>
			<style type="text/css">
				table.showMe { display:none; }
				div.TableDiv:hover table.showMe { display:block; }
			</style>
		</head>
		<body>
			<h4>Banks</h4>
			<table border="0">
				<tr bgcolor="#9acd32">
					<th align="left">Number</th>
					<th align="left">Name</th>
					<th align="left">Patch Mappings (Switch - Patch - Patch Name)</th>
				</tr>
				<xsl:for-each select="MidiControlSettings/banks/bank">
					<tr>
						<td valign="top"><xsl:value-of select="@number"/></td>
						<td valign="top"><xsl:value-of select="@name"/></td>
						<td valign="top">
							<div class="TableDiv">
								<xsl:text>&#160;</xsl:text>
							<table class="showMe" border="0">
								<xsl:for-each select="PatchMap">
									<tr>
										<xsl:variable name="patchNumber" select="@patch"/>
										<td><xsl:value-of select="@switch"/><xsl:text>&#160;&#160;</xsl:text></td>
										<td><xsl:value-of select="@patch"/></td>
										<td><xsl:element name="a">
											<xsl:attribute name="href">
												<xsl:text>#p</xsl:text>
												<xsl:value-of select="@patch"/>
											</xsl:attribute>
											<!--<xsl:text>&#160;</xsl:text>-->
											<xsl:value-of select="/MidiControlSettings/patches/patch[@number=$patchNumber]/@name"/>
										</xsl:element>
										</td>
									</tr>
								</xsl:for-each>
							</table>
							</div>
						</td>
					</tr>
				</xsl:for-each>
			</table>

			<h4>Patches</h4>
			<table border="0">
				<tr bgcolor="#9acd32">
					<th align="left">Number</th>
					<th align="left">Name</th>
					<th align="left">Type</th>
				</tr>
				<xsl:for-each select="MidiControlSettings/patches/patch">
					<tr>
						<td>
							<xsl:element name="a">
								<xsl:attribute name="name">
									<xsl:text>p</xsl:text>
									<xsl:value-of select="@number"/>
								</xsl:attribute>
							</xsl:element>
							<xsl:value-of select="@number"/>
						</td>
						<td><xsl:value-of select="@name"/></td>
						<td><xsl:value-of select="@type"/></td>
					</tr>
				</xsl:for-each>
			</table>

			<h4>Meta Patches</h4>
			<table border="0">
				<tr bgcolor="#9acd32">
					<th align="left">Number</th>
					<th align="left">Name</th>
					<th align="left">Action</th>
				</tr>
				<xsl:for-each select="MidiControlSettings/patches/engineMetaPatch">
					<tr>
						<td>
							<xsl:element name="a">
								<xsl:attribute name="name">
									<xsl:text>p</xsl:text>
									<xsl:value-of select="@number"/>
								</xsl:attribute>
							</xsl:element>
							<xsl:value-of select="@number"/>
						</td>
						<td><xsl:value-of select="@name"/></td>
						<td><xsl:value-of select="@action"/></td>
					</tr>
				</xsl:for-each>
			</table>
		</body>
	</html>
</xsl:template>
</xsl:stylesheet>
