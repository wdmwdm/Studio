#include "shapehelper.h"
#include "linemeshbuilder.h"
#include <qmath.h>

namespace iris
{

MeshPtr ShapeHelper::createWireCube()
{
    LineMeshBuilder builder;
    return MeshPtr();
}

MeshPtr ShapeHelper::createWireSphere()
{
    LineMeshBuilder builder;


    int divisions = 36;
    float arcWidth = 360.0f/divisions;

    // XY plane
    for(int i=0;i<divisions;i++)
    {
        float angle = i * arcWidth;
        QVector3D a = QVector3D(qSin(qDegreesToRadians(angle)), qCos(qDegreesToRadians(angle)), 0);

        angle = (i+1) * arcWidth;
        QVector3D b = QVector3D(qSin(qDegreesToRadians(angle)), qCos(qDegreesToRadians(angle)), 0);

        builder.addLine(a, b);
    }

    // XZ plane
    for(int i=0;i<divisions;i++)
    {
        float angle = i * arcWidth;
        QVector3D a = QVector3D(qSin(qDegreesToRadians(angle)), 0, qCos(qDegreesToRadians(angle)));

        angle = (i+1) * arcWidth;
        QVector3D b = QVector3D(qSin(qDegreesToRadians(angle)), 0, qCos(qDegreesToRadians(angle)));

        builder.addLine(a, b);
    }

    // YZ plane
    for(int i=0;i<divisions;i++)
    {
        float angle = i * arcWidth;
        QVector3D a = QVector3D(0, qSin(qDegreesToRadians(angle)), qCos(qDegreesToRadians(angle)));

        angle = (i+1) * arcWidth;
        QVector3D b = QVector3D(0, qSin(qDegreesToRadians(angle)), qCos(qDegreesToRadians(angle)));

        builder.addLine(a, b);
    }

    return builder.build();
}

}
