/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "distancefield_glyphnode_p.h"
#include <qmath.h>

class DistanceFieldTextMaterialShader : public AbstractMaterialShader
{
public:
    DistanceFieldTextMaterialShader();

    virtual void updateState(Renderer *renderer, AbstractMaterial *newEffect, AbstractMaterial *oldEffect, Renderer::Updates updates);
    virtual char const *const *attributeNames() const;

protected:
    virtual void initialize();
    virtual const char *vertexShader() const;
    virtual const char *fragmentShader() const;

    void updateAlphaRange();

    qreal m_fontScale;
    qreal m_matrixScale;

    int m_matrix_id;
    int m_alphaMin_id;
    int m_alphaMax_id;
    int m_color_id;
};

const char *DistanceFieldTextMaterialShader::vertexShader() const {
    return
        "uniform highp mat4 matrix;                     \n"
        "attribute highp vec4 vCoord;                   \n"
        "attribute highp vec2 tCoord;                   \n"
        "varying highp vec2 sampleCoord;                \n"
        "void main() {                                  \n"
        "     sampleCoord = tCoord;                     \n"
        "     gl_Position = matrix * vCoord;            \n"
        "}";
}

const char *DistanceFieldTextMaterialShader::fragmentShader() const {
    return
        "varying highp vec2 sampleCoord;                                             \n"
        "uniform sampler2D texture;                                                  \n"
        "uniform lowp vec4 color;                                                    \n"
        "uniform highp float alphaMin;                                               \n"
        "uniform highp float alphaMax;                                               \n"
        "void main() {                                                               \n"
        "    gl_FragColor = color * smoothstep(alphaMin,                             \n"
        "                                      alphaMax,                             \n"
        "                                      texture2D(texture, sampleCoord).a);   \n"
        "}";
}

char const *const *DistanceFieldTextMaterialShader::attributeNames() const {
    static char const *const attr[] = { "vCoord", "tCoord", 0 };
    return attr;
}

DistanceFieldTextMaterialShader::DistanceFieldTextMaterialShader()
    : m_fontScale(1.0)
    , m_matrixScale(1.0)
{
}

void DistanceFieldTextMaterialShader::updateAlphaRange()
{
    qreal combinedScale = m_fontScale * m_matrixScale;
    qreal alphaMin = qBound(0.0, 0.5 - 0.07 / combinedScale, 0.5);
    qreal alphaMax = qBound(0.5, 0.5 + 0.07 / combinedScale, 1.0);
    m_program.setUniformValue(m_alphaMin_id, GLfloat(alphaMin));
    m_program.setUniformValue(m_alphaMax_id, GLfloat(alphaMax));
}

void DistanceFieldTextMaterialShader::initialize()
{
    AbstractMaterialShader::initialize();
    m_matrix_id = m_program.uniformLocation("matrix");
    m_color_id = m_program.uniformLocation("color");
    m_alphaMin_id = m_program.uniformLocation("alphaMin");
    m_alphaMax_id = m_program.uniformLocation("alphaMax");
}

void DistanceFieldTextMaterialShader::updateState(Renderer *renderer, AbstractMaterial *newEffect, AbstractMaterial *oldEffect, Renderer::Updates updates)
{
    Q_ASSERT(oldEffect == 0 || newEffect->type() == oldEffect->type());
    DistanceFieldTextMaterial *material = static_cast<DistanceFieldTextMaterial *>(newEffect);
    DistanceFieldTextMaterial *oldMaterial = static_cast<DistanceFieldTextMaterial *>(oldEffect);

    if (oldMaterial == 0
           || material->color() != oldMaterial->color()
           || (updates & Renderer::UpdateOpacity)) {
        QVector4D color(material->color().redF(), material->color().greenF(),
                        material->color().blueF(), material->color().alphaF());
        color *= renderer->renderOpacity();
        m_program.setUniformValue(m_color_id, color);
    }

    bool updateRange = false;
    if (oldMaterial == 0 || material->scale() != oldMaterial->scale()) {
        m_fontScale = material->scale();
        updateRange = true;
    }
    if (updates & Renderer::UpdateMatrices) {
        m_program.setUniformValue(m_matrix_id, renderer->combinedMatrix());
        m_matrixScale = qSqrt(renderer->modelViewMatrix().top().determinant());
        updateRange = true;
    }
    if (updateRange)
        updateAlphaRange();

    Q_ASSERT(!material->texture().isNull());

    Q_ASSERT(oldMaterial == 0 || !oldMaterial->texture().isNull());
    if (oldMaterial == 0
            || oldMaterial->texture()->textureId() != material->texture()->textureId()) {
        renderer->setTexture(0, material->texture());

        if (material->updateTextureFiltering()) {
            // Set the mag/min filters to be linear. We only need to do this when the texture
            // has been recreated.
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
    }
}

DistanceFieldTextMaterial::DistanceFieldTextMaterial()
    : m_texture(0), m_scale(1.0), m_dirtyTexture(false)
{
   setFlag(Blending, true);
}

DistanceFieldTextMaterial::~DistanceFieldTextMaterial()
{
}

AbstractMaterialType *DistanceFieldTextMaterial::type() const
{
    static AbstractMaterialType type;
    return &type;
}

AbstractMaterialShader *DistanceFieldTextMaterial::createShader() const
{
    return new DistanceFieldTextMaterialShader;
}

int DistanceFieldTextMaterial::compare(const AbstractMaterial *o) const
{
    Q_ASSERT(o && type() == o->type());
    const DistanceFieldTextMaterial *other = static_cast<const DistanceFieldTextMaterial *>(o);
    if (m_scale != other->m_scale) {
        qreal s1 = m_scale, s2 = other->m_scale;
        return int(s2 < s1) - int(s1 < s2);
    }
    QRgb c1 = m_color.rgba();
    QRgb c2 = other->m_color.rgba();
    return int(c2 < c1) - int(c1 < c2);
}


class DistanceFieldStyledTextMaterialShader : public DistanceFieldTextMaterialShader
{
public:
    DistanceFieldStyledTextMaterialShader();

    virtual void updateState(Renderer *renderer, AbstractMaterial *newEffect, AbstractMaterial *oldEffect, Renderer::Updates updates);

protected:
    virtual void initialize();
    virtual const char *fragmentShader() const = 0;

    void updateStyleRange();

    int m_styleAlphaMin0_id;
    int m_styleAlphaMin1_id;
    int m_styleColor_id;
    int m_pixelSize_id;
};

DistanceFieldStyledTextMaterialShader::DistanceFieldStyledTextMaterialShader()
    : DistanceFieldTextMaterialShader()
{
}

void DistanceFieldStyledTextMaterialShader::updateStyleRange()
{
    qreal styleLimit = 0.4;

    qreal combinedScale = m_fontScale * m_matrixScale;
    qreal alphaMin = qBound(0.0, 0.5 - 0.07 / combinedScale, 0.5);
    qreal styleAlphaMin0 = qBound(0.0, styleLimit - 0.07 / combinedScale, styleLimit);
    qreal styleAlphaMin1 = qBound(styleLimit, styleLimit + 0.07 / combinedScale, alphaMin);
    m_program.setUniformValue(m_styleAlphaMin0_id, GLfloat(styleAlphaMin0));
    m_program.setUniformValue(m_styleAlphaMin1_id, GLfloat(styleAlphaMin1));
}

void DistanceFieldStyledTextMaterialShader::initialize()
{
    DistanceFieldTextMaterialShader::initialize();
    m_styleColor_id = m_program.uniformLocation("styleColor");
    m_styleAlphaMin0_id = m_program.uniformLocation("styleAlphaMin0");
    m_styleAlphaMin1_id = m_program.uniformLocation("styleAlphaMin1");
    m_pixelSize_id = m_program.uniformLocation("pixelSize");
}

void DistanceFieldStyledTextMaterialShader::updateState(Renderer *renderer, AbstractMaterial *newEffect, AbstractMaterial *oldEffect, Renderer::Updates updates)
{
    DistanceFieldTextMaterialShader::updateState(renderer, newEffect, oldEffect, updates);

    Q_ASSERT(oldEffect == 0 || newEffect->type() == oldEffect->type());
    DistanceFieldOutlineTextMaterial *material = static_cast<DistanceFieldOutlineTextMaterial *>(newEffect);
    DistanceFieldOutlineTextMaterial *oldMaterial = static_cast<DistanceFieldOutlineTextMaterial *>(oldEffect);

    if (oldMaterial == 0
           || material->styleColor() != oldMaterial->styleColor()
           || (updates & Renderer::UpdateOpacity)) {
        QVector4D color(material->styleColor().redF(), material->styleColor().greenF(),
                        material->styleColor().blueF(), material->styleColor().alphaF());
        color *= renderer->renderOpacity();
        m_program.setUniformValue(m_styleColor_id, color);
    }

    if (oldMaterial == 0 || material->scale() != oldMaterial->scale() || updates & Renderer::UpdateMatrices)
        updateStyleRange();

    if (oldMaterial == 0
            || oldMaterial->texture()->textureSize() != material->texture()->textureSize()) {
        m_program.setUniformValue(m_pixelSize_id, GLfloat(1.0 / material->texture()->textureSize().height()));
    }
}

DistanceFieldStyledTextMaterial::DistanceFieldStyledTextMaterial()
    : DistanceFieldTextMaterial()
{
}

DistanceFieldStyledTextMaterial::~DistanceFieldStyledTextMaterial()
{
}

int DistanceFieldStyledTextMaterial::compare(const AbstractMaterial *o) const
{
    Q_ASSERT(o && type() == o->type());
    const DistanceFieldStyledTextMaterial *other = static_cast<const DistanceFieldStyledTextMaterial *>(o);
    if (m_styleColor != other->m_styleColor) {
        QRgb c1 = m_styleColor.rgba();
        QRgb c2 = other->m_styleColor.rgba();
        return int(c2 < c1) - int(c1 < c2);
    }
    return DistanceFieldTextMaterial::compare(o);
}


class DistanceFieldOutlineTextMaterialShader : public DistanceFieldStyledTextMaterialShader
{
public:
    DistanceFieldOutlineTextMaterialShader();

protected:
    virtual const char *fragmentShader() const;
};

const char *DistanceFieldOutlineTextMaterialShader::fragmentShader() const {
    return
            "varying highp vec2 sampleCoord;                                             \n"
            "uniform sampler2D texture;                                                  \n"
            "uniform lowp vec4 color;                                                    \n"
            "uniform lowp vec4 styleColor;                                               \n"
            "uniform highp float alphaMin;                                               \n"
            "uniform highp float alphaMax;                                               \n"
            "uniform highp float styleAlphaMin0;                                         \n"
            "uniform highp float styleAlphaMin1;                                         \n"
            "uniform highp float pixelSize;                                              \n"
            "void main() {                                                               \n"
            "    mediump float d = texture2D(texture, sampleCoord).a;                    \n"
            "    highp vec4 o = color * smoothstep(alphaMin,alphaMax,d);                 \n"
            "    if (d <= alphaMax && d >= styleAlphaMin0) {                             \n"
            "        mediump float mu = 1.0;                                             \n"
            "        if (d <= styleAlphaMin1) {                                          \n"
            "            mu = smoothstep(styleAlphaMin0, styleAlphaMin1, d);             \n"
            "            o = styleColor * mu;                                            \n"
            "        } else {                                                            \n"
            "            mu = smoothstep(alphaMax, alphaMin, d);                         \n"
            "            o = mix(color, styleColor, mu);                                 \n"
            "        }                                                                   \n"
            "    }                                                                       \n"
            "    gl_FragColor = o;                                                       \n"
            "}";
}

DistanceFieldOutlineTextMaterialShader::DistanceFieldOutlineTextMaterialShader()
    : DistanceFieldStyledTextMaterialShader()
{
}


DistanceFieldOutlineTextMaterial::DistanceFieldOutlineTextMaterial()
    : DistanceFieldStyledTextMaterial()
{
}

DistanceFieldOutlineTextMaterial::~DistanceFieldOutlineTextMaterial()
{
}

AbstractMaterialType *DistanceFieldOutlineTextMaterial::type() const
{
    static AbstractMaterialType type;
    return &type;
}

AbstractMaterialShader *DistanceFieldOutlineTextMaterial::createShader() const
{
    return new DistanceFieldOutlineTextMaterialShader;
}


class DistanceFieldSunkenTextMaterialShader : public DistanceFieldStyledTextMaterialShader
{
public:
    DistanceFieldSunkenTextMaterialShader();

protected:
    virtual const char *fragmentShader() const;
};

DistanceFieldSunkenTextMaterialShader::DistanceFieldSunkenTextMaterialShader()
    : DistanceFieldStyledTextMaterialShader()
{
}

const char *DistanceFieldSunkenTextMaterialShader::fragmentShader() const {
    return
            "varying highp vec2 sampleCoord;                                                   \n"
            "uniform sampler2D texture;                                                        \n"
            "uniform lowp vec4 color;                                                          \n"
            "uniform lowp vec4 styleColor;                                                     \n"
            "uniform highp float alphaMin;                                                     \n"
            "uniform highp float alphaMax;                                                     \n"
            "uniform highp float styleAlphaMin0;                                               \n"
            "uniform highp float styleAlphaMin1;                                               \n"
            "uniform highp float pixelSize;                                                    \n"
            "void main() {                                                                     \n"
            "    mediump float d = texture2D(texture, sampleCoord).a;                          \n"
            "    highp vec4 o = color * smoothstep(alphaMin,alphaMax,d);                       \n"
            "    mediump float outline = texture2D(texture, sampleCoord + vec2(0.0, pixelSize)).a; \n"
            "    if (d <= alphaMax && d >= styleAlphaMin0 && outline >= 0.5) {                 \n"
            "        mediump float mu = 1.0;                                                   \n"
            "        if (d <= styleAlphaMin1) {                                                \n"
            "            mu = smoothstep(styleAlphaMin0, styleAlphaMin1, d);                   \n"
            "            o = styleColor * mu;                                                  \n"
            "        } else {                                                                  \n"
            "            mu = smoothstep(alphaMax, alphaMin, d);                               \n"
            "            o = mix(color, styleColor, mu);                                       \n"
            "        }                                                                         \n"
            "    }                                                                             \n"
            "    gl_FragColor = o;                                                             \n"
            "}";
}

DistanceFieldSunkenTextMaterial::DistanceFieldSunkenTextMaterial()
    : DistanceFieldStyledTextMaterial()
{
}

DistanceFieldSunkenTextMaterial::~DistanceFieldSunkenTextMaterial()
{
}

AbstractMaterialType *DistanceFieldSunkenTextMaterial::type() const
{
    static AbstractMaterialType type;
    return &type;
}

AbstractMaterialShader *DistanceFieldSunkenTextMaterial::createShader() const
{
    return new DistanceFieldSunkenTextMaterialShader;
}


class DistanceFieldRaisedTextMaterialShader : public DistanceFieldStyledTextMaterialShader
{
public:
    DistanceFieldRaisedTextMaterialShader();

protected:
    virtual const char *fragmentShader() const;
};

DistanceFieldRaisedTextMaterialShader::DistanceFieldRaisedTextMaterialShader()
    : DistanceFieldStyledTextMaterialShader()
{
}

const char *DistanceFieldRaisedTextMaterialShader::fragmentShader() const {
    return
            "varying highp vec2 sampleCoord;                                                   \n"
            "uniform sampler2D texture;                                                        \n"
            "uniform lowp vec4 color;                                                          \n"
            "uniform lowp vec4 styleColor;                                                     \n"
            "uniform highp float alphaMin;                                                     \n"
            "uniform highp float alphaMax;                                                     \n"
            "uniform highp float styleAlphaMin0;                                               \n"
            "uniform highp float styleAlphaMin1;                                               \n"
            "uniform highp float pixelSize;                                                    \n"
            "void main() {                                                                     \n"
            "    mediump float d = texture2D(texture, sampleCoord).a;                          \n"
            "    highp vec4 o = color * smoothstep(alphaMin,alphaMax,d);                       \n"
            "    mediump float outline = texture2D(texture, sampleCoord - vec2(0.0, pixelSize)).a; \n"
            "    if (d <= alphaMax && d >= styleAlphaMin0 && outline >= 0.5) {                 \n"
            "        mediump float mu = 1.0;                                                   \n"
            "        if (d <= styleAlphaMin1) {                                                \n"
            "            mu = smoothstep(styleAlphaMin0, styleAlphaMin1, d);                   \n"
            "            o = styleColor * mu;                                                  \n"
            "        } else {                                                                  \n"
            "            mu = smoothstep(alphaMax, alphaMin, d);                               \n"
            "            o = mix(color, styleColor, mu);                                       \n"
            "        }                                                                         \n"
            "    }                                                                             \n"
            "    gl_FragColor = o;                                                             \n"
            "}";
}

DistanceFieldRaisedTextMaterial::DistanceFieldRaisedTextMaterial()
    : DistanceFieldStyledTextMaterial()
{
}

DistanceFieldRaisedTextMaterial::~DistanceFieldRaisedTextMaterial()
{
}

AbstractMaterialType *DistanceFieldRaisedTextMaterial::type() const
{
    static AbstractMaterialType type;
    return &type;
}

AbstractMaterialShader *DistanceFieldRaisedTextMaterial::createShader() const
{
    return new DistanceFieldRaisedTextMaterialShader;
}