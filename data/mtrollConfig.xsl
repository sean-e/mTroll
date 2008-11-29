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
				<xsl:variable name="displayStrings" select="0" />
				<tr bgcolor="#9acd32">
					<th>Number</th>
					<th>Name</th>
					<th>Type</th>
					<xsl:if test="$displayStrings = 1">
						<th>String A</th>
						<th>String B</th>
					</xsl:if>
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
						<xsl:if test="$displayStrings = 1">
							<td><xsl:value-of select="midiByteString[@name='A']"/></td>
							<td><xsl:value-of select="midiByteString[@name='B']"/></td>
						</xsl:if>
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
					<th>Load Bank Target</th>
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
						<xsl:variable name="bankNum" select="@bankNumber" />
						<xsl:if test="$bankNum!=''">
							<td>
								<xsl:element name="a">
									<xsl:attribute name="href">
										<xsl:text>#b</xsl:text>
										<xsl:value-of select="$bankNum"/>
									</xsl:attribute>
									<xsl:value-of select="$bankNum"/>
									<xsl:text>&#160;</xsl:text>
									<xsl:value-of select="/MidiControlSettings/banks/bank[@number=$bankNum]/@name"/>
								</xsl:element>
							</td>
						</xsl:if>
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
						<td>
							<xsl:element name="a">
								<xsl:attribute name="name">
									<xsl:text>b</xsl:text>
									<xsl:value-of select="@number"/>
								</xsl:attribute>
							</xsl:element>
							<xsl:value-of select="@number"/>
						</td>
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
											<td>
												<xsl:element name="a">
													<xsl:attribute name="href">
														<xsl:text>#p</xsl:text>
														<xsl:value-of select="@patch"/>
													</xsl:attribute>
													<xsl:variable name="patchName" select="/MidiControlSettings/patches/patch[@number=$patchNumber]/@name"/>
													<xsl:choose>
														<xsl:when test="$patchName != ''">
															<xsl:value-of select="$patchName" />
														</xsl:when>
														<xsl:otherwise>
															<xsl:value-of select="/MidiControlSettings/patches/engineMetaPatch[@number=$patchNumber]/@name"/>
														</xsl:otherwise>
													</xsl:choose>
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

			<!-- add lines to that links to banks will cause target to be first vis line -->
			<div>
				<br />
				<br />
				<br />
				<br />
				<br />
				<br />
				<br />
				<br />
				<br />
				<br />
				<br />
				<br />
				<br />
				<br />
				<br />
				<br />
				<br />
				<br />
				<br />
				<br />
			</div>
		</body>
	</html>
</xsl:template>
</xsl:stylesheet>
