# -*- coding: utf-8 -*-

"""
***************************************************************************
    SpatialiteExecuteSQL.py
    ---------------------
    Date                 : October 2016
    Copyright            : (C) 2016 by Mathieu Pellerin
    Email                : nirvn dot asia at gmail dot com
***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************
"""
from builtins import str

__author__ = 'Mathieu Pellerin'
__date__ = 'October 2016'
__copyright__ = '(C) 2016, Mathieu Pellerin'

# This will get replaced with a git SHA1 when you do a git archive

__revision__ = '$Format:%H$'

from qgis.core import (QgsDataSourceUri,
                       QgsProcessingException,
                       QgsProcessingParameterVectorLayer,
                       QgsProcessingParameterString)

from processing.algs.qgis.QgisAlgorithm import QgisAlgorithm
from processing.tools import spatialite


class SpatialiteExecuteSQL(QgisAlgorithm):

    DATABASE = 'DATABASE'
    SQL = 'SQL'

    def group(self):
        return self.tr('Database')

    def __init__(self):
        super().__init__()

    def initAlgorithm(self, config=None):
        self.addParameter(QgsProcessingParameterVectorLayer(self.DATABASE, self.tr('File Database'), [], False, False))
        self.addParameter(QgsProcessingParameterString(self.SQL, self.tr('SQL query'), '', True))

    def name(self):
        return 'spatialiteexecutesql'

    def displayName(self):
        return self.tr('execute SQL inside spatialite with no output')

    def processAlgorithm(self, parameters, context, feedback):
        database = self.parameterAsVectorLayer(parameters, self.DATABASE, context)
        databaseuri = database.dataProvider().dataSourceUri()
        uri = QgsDataSourceUri(databaseuri)
        if uri.database() is '':
            if '|layerid' in databaseuri:
                databaseuri = databaseuri[:databaseuri.find('|layerid')]
            uri = QgsDataSourceUri('dbname=\'%s\'' % (databaseuri))
        db = spatialite.GeoDB(uri)
        sql = self.parameterAsString(parameters, self.SQL, context).replace('\n', ' ')
        try:
            db._exec_sql_and_commit(str(sql))
        except spatialite.DbError as e:
            raise QgsProcessingException(
                self.tr('Error executing SQL:\n{0}').format(str(e)))

        return {}
