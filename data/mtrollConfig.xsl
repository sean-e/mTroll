<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:template match="/">
	<html>
		<head>
			<style type="text/css">
				table.PatchMapTableCls { display:none; }
				div.TableDiv:hover table.PatchMapTableCls { display:block; }
				table { border:0; }
				th { text-align:left; }
				td { vertical-align:top; }
			</style>
		</head>
		<body>
			<h4>Contents</h4>
			<div>
				<a href="#banks">Banks</a><br />
				<a href="#patches">Patches</a><br />
				<a href="#metapatches">Meta Patches</a><br />
			</div>
			
			<h4><a name="patches">Patches</a></h4>
			<table>
				<tr bgcolor="#9acd32">
					<th>Number</th>
					<th>Name</th>
					<th>Type</th>
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
			<br />
			<br />

			<h4><a name="metapatches">Meta Patches</a></h4>
			<table>
				<tr bgcolor="#9acd32">
					<th>Number</th>
					<th>Name</th>
					<th>Action</th>
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
			<br />
			<br />

			<h4><a name="banks">Banks</a></h4>
			<table>
				<tr bgcolor="#9acd32">
					<th>Number</th>
					<th>Name</th>
					<th>Patch Mappings (Switch - Patch - Patch Name)</th>
				</tr>
				<xsl:for-each select="MidiControlSettings/banks/bank">
					<tr>
						<td><xsl:value-of select="@number"/></td>
						<td><xsl:value-of select="@name"/></td>
						<td>
							<div class="TableDiv">
								<span>[view]</span>
								<table class="PatchMapTableCls">
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

		</body>
	</html>
</xsl:template>
</xsl:stylesheet>
