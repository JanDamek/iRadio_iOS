//
//  Categorie.h
//  iRadio
//
//  Created by Jan Damek on 12.01.14.
//  Copyright (c) 2014 Jan Damek. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>


@interface Categorie : NSManagedObject

@property (nonatomic, retain) NSString * name;
@property (nonatomic, retain) NSNumber * user_def;
@property (nonatomic, retain) NSDate * timeStamp;
@property (nonatomic, retain) NSSet *radios_rel;
@end

@interface Categorie (CoreDataGeneratedAccessors)

- (void)addRadios_relObject:(NSManagedObject *)value;
- (void)removeRadios_relObject:(NSManagedObject *)value;
- (void)addRadios_rel:(NSSet *)values;
- (void)removeRadios_rel:(NSSet *)values;

@end
