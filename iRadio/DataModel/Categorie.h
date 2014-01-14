//
//  Categorie.h
//  iRadioHD
//
//  Created by Jan Damek on 14.01.14.
//  Copyright (c) 2014 Jan Damek. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Radio;

@interface Categorie : NSManagedObject

@property (nonatomic, retain) NSString * title;
@property (nonatomic, retain) NSNumber * user_def;
@property (nonatomic, retain) NSDate * timeStamp;
@property (nonatomic, retain) NSNumber * category_id;
@property (nonatomic, retain) NSSet *radios_rel;
@end

@interface Categorie (CoreDataGeneratedAccessors)

- (void)addRadios_relObject:(Radio *)value;
- (void)removeRadios_relObject:(Radio *)value;
- (void)addRadios_rel:(NSSet *)values;
- (void)removeRadios_rel:(NSSet *)values;

@end
