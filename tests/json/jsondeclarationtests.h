/* This file is part of KDevelop
   Copyright 2012 Olivier de Gaalon <olivier.jg@gmail.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef JSONDECLARATIONTESTS_H
#define JSONDECLARATIONTESTS_H

#include "language/duchain/ducontext.h"
#include "language/duchain/declaration.h"
#include "language/duchain/indexeddeclaration.h"
#include "language/duchain/identifier.h"
#include "language/duchain/abstractfunctiondeclaration.h"
#include "language/duchain/types/typeutils.h"
#include "language/duchain/types/identifiedtype.h"
#include "jsontesthelpers.h"

/**
 * JSON Object Specification:
 *   DeclTestObject: Mapping of (string) declaration test names to values
 *   TypeTestObject: Mapping of (string) type test names to values
 *   CtxtTestObject: Mapping of (string) context test names to values
 *
 * Quick Reference:
 *   useCount : int
 *   identifier : string
 *   qualifiedIdentifier : string
 *   internalContext : CtxtTestObject
 *   internalFunctionContext : CtxtTestObject
 *   type : TypeTestObject
 *   unaliasedType : TypeTestObject
 *   targetType : TypeTestObject
 *   identifiedTypeQid : string
 */

namespace KDevelop
{
  template<>
  QString TestSuite<Declaration*>::objectInformation(Declaration *decl)
  {
    return QString("(Declaration on line %1 in %2)")
        .arg(decl->range().start.line + 1)
        .arg(decl->topContext()->url().str());
  }

namespace DeclarationTests
{

using namespace JsonTestHelpers;

///JSON type: int
///@returns whether the declaration's number of uses matches the given value
DeclarationTest(useCount)
{
  return compareValues(decl->uses().size(), value, "Declaration's use count ");
}
///JSON type: string
///@returns whether the declaration's identifier matches the given value
DeclarationTest(identifier)
{
  return compareValues(decl->identifier().toString(), value, "Declaration's identifier");
}
///JSON type: string
///@returns whether the declaration's qualified identifier matches the given value
DeclarationTest(qualifiedIdentifier)
{
  return compareValues(decl->qualifiedIdentifier().toString(), value, "Declaration's qualified identifier");
}
///JSON type: CtxtTestObject
///@returns whether the tests for the declaration's internal context pass
DeclarationTest(internalContext)
{
  return testObject(decl->internalContext(), value, "Declaration's internal context");
}
///JSON type: CtxtTestObject
///@returns whether the tests for the declaration's internal function context pass
DeclarationTest(internalFunctionContext)
{
  const QString NO_INTERNAL_CTXT = "%1 has no internal function context.";
  AbstractFunctionDeclaration *absFuncDecl = dynamic_cast<AbstractFunctionDeclaration*>(decl);
  if (!absFuncDecl || !absFuncDecl->internalFunctionContext())
    return NO_INTERNAL_CTXT.arg(decl->qualifiedIdentifier().toString());
  return testObject(decl->internalContext(), value, "Declaration's internal function context");
}
/*FIXME: The type functions need some renaming and moving around
 * Some (all?) functions from cpp's TypeUtils should be moved to the kdevplatform type utils
 * targetType is exactly like realType except it also tosses pointers
 * shortenTypeForViewing should go to type utils (and it's broken, it places const to the left of all *'s when it should be left or right of the type)
 * UnaliasedType seems to be unable to understand aliases involving templates, perhaps a cpp version is in order
 */
///JSON type: TypeTestObject
///@returns whether the tests for the declaration's type pass
DeclarationTest(type)
{
  return testObject(decl->abstractType(), value, "Declaration's type");
}
///JSON type: TypeTestObject
///@returns whether the tests for the declaration's unaliased type pass (TypeUtils::unaliasedType)
DeclarationTest(unaliasedType)
{
  return testObject(TypeUtils::unAliasedType(decl->abstractType()), value, "Declaration's unaliased type");
}
///JSON type: TypeTestObject
///@returns whether the tests for the declaration's target type pass (TypeUtils::targetType)
DeclarationTest(targetType)
{
  return testObject(TypeUtils::targetType(decl->abstractType(), decl->topContext()), value, "Declaration's target type");
}
///JSON type: string
///@returns whether the declaration's type's declaration can be identified and if it's qualified identifier matches the given value
DeclarationTest(identifiedTypeQid)
{
  VERIFY_TYPE(QString);
  const QString UN_ID_ERROR = "Unable to identify declaration of type \"%1\".";
  AbstractType::Ptr type = decl->abstractType();
  IdentifiedType* idType = dynamic_cast<IdentifiedType*>(type.unsafeData());
  Declaration* idDecl = idType ? idType->declaration(decl->topContext()) : 0;
  if (!idDecl)
    return UN_ID_ERROR.arg(type->toString());

  return compareValues(idDecl->qualifiedIdentifier().toString(), value, "Declaration's identified type");
}

}

}

#endif //JSONDECLARATIONTESTS_H